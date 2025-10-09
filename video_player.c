// video_player.c
#include <kos.h>
#include <kmg/kmg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_player.h"
#include "audio.h"
#include "sprite.h"
#include "scene.h"
#include "menu.h"
#define PL_MPEG_IMPLEMENTATION
#include "mpegDC.h"

void render_video_frame();
void yuv420_to_yuv422(plm_frame_t *frame, uint16_t *vram);
void on_video_fadeout_complete();

void play_video(const char *filenameVideo, const char *filenameAudio)
{
    uint32_t prev_secs = 0, prev_ms = 0;
    // --- Cargar video ---
    mpeg = plm_create_with_filename(filenameVideo);
    if (!mpeg)
    {
        printf("[VIDEO_PLAYER] No se pudo abrir el video %s\n", filenameVideo);
        return;
    }
    plm_set_audio_enabled(mpeg, 0); // Audio externo

    // Reproducir WAV externo
    audio_play_music(filenameAudio, 0);

    uint64_t video_start_ms = timer_ms_gettime64();
    plm_frame_t *frame = NULL;
    int final_frame_rendered = 0; // flag: ya renderizamos el último frame convertido en video_tex

    while (playing_video || g_fade.active)
    {
        cont_state_t *st = (cont_state_t *)maple_dev_status(maple_enum_type(0, MAPLE_FUNC_CONTROLLER));
        uint32 pressed = st ? st->buttons : 0;

        uint32_t now_secs = 0, now_ms = 0;
        timer_ms_gettime(&now_secs, &now_ms);
        int delta_ms = (int)((now_secs - prev_secs) * 1000 + (now_ms - prev_ms));
        if (delta_ms < 0)
            delta_ms = 0;
        if (delta_ms > 200)
            delta_ms = 200;
        prev_secs = now_secs;
        prev_ms = now_ms;

        // --- Actualiza lógica de fades ---
        fade_update(delta_ms);

        // Decodificar siguiente frame (puede devolver NULL cuando el video terminó)
        frame = plm_decode_video(mpeg);

        if (!frame || (pressed & CONT_A))
        {
            // Video finalizado: si no hay fade activo, iniciar fade-out una vez
            if (!g_fade.active)
            {
                printf("Se presiono A (skip)\n");
                g_fade.active = 1;
                g_fade.elapsed_ms = 0;
                g_fade.duration_ms = 600;
                g_fade.alpha = 0.0f;
                g_fade.fading_out = 1;
                playing_video = 0;
                g_fade.on_complete = on_video_fadeout_complete;
                printf("[play_intro_video] video terminó -> iniciando fade out\n");
            }
            if (!frame)
            {
                g_fade.on_complete = on_video_fadeout_complete;
                break;
            }
            // No hay frame nuevo; vamos a renderizar **el último frame** que ya esté en video_tex.
            // No llamamos a yuv420_to_yuv422 ni usamos 'frame'.
            // Indicamos que estamos renderizando el final (opcional)
            final_frame_rendered = 1;
        }
        else
        {
            // Tenemos frame válido: convertir y subir a VRAM
            uint64_t target_ms = video_start_ms + (uint64_t)(frame->time * 1000.0);
            // esperar hasta el pts del frame
            while (timer_ms_gettime64() < target_ms)
                thd_sleep(1);

            yuv420_to_yuv422(frame, (uint16_t *)video_tex);
        }

        // Renderizar (usamos lo que haya en video_tex; si frame era NULL se mostrará el último)
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_set_bg_color(0.0f, 0.0f, 0.0f);

        pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                         PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED,
                         VIDEO_W, VIDEO_H, video_tex, PVR_FILTER_NONE);
        pvr_poly_compile(&hdr, &cxt);

        pvr_list_begin(PVR_LIST_OP_POLY);
        render_video_frame(frame); // tu render usa el video_tex, no debería depender de frame si es NULL
        pvr_list_finish();

        pvr_list_begin(PVR_LIST_TR_POLY);
        if (g_fade.alpha > 0.0f)
            draw_fade_overlay(g_fade.alpha);
        pvr_list_finish();

        pvr_scene_finish();
    }
}

void on_video_fadeout_complete()
{
    printf("on_video_fadeout_complete\n");
    if(state == STATE_VIDEO){
        state = STATE_MENU; // Cambiar al menú
        menu_active = 1;
        menu_init(&main_menu);
    }

    playing_video = 0;
    //  No destruimos video ni textura aquí: lo hacemos en el loop, una vez que el menú empieza a renderizar
}

// -------------------
// Renderiza el video escalado a pantalla completa
// -------------------
void render_video_frame(plm_frame_t *video_frame)
{
    pvr_prim(&hdr, sizeof(hdr));

    vert[0].x = 0;
    vert[0].y = 0;
    vert[0].z = 1;
    vert[0].u = 0.0f;
    vert[0].v = 0.0f;
    vert[0].argb = 0xFFFFFFFF;
    vert[0].flags = PVR_CMD_VERTEX;

    vert[1].x = SCREEN_W;
    vert[1].y = 0;
    vert[1].z = 1;
    vert[1].u = 1.0f;
    vert[1].v = 0.0f;
    vert[1].argb = 0xFFFFFFFF;
    vert[1].flags = PVR_CMD_VERTEX;

    vert[2].x = 0;
    vert[2].y = SCREEN_H;
    vert[2].z = 1;
    vert[2].u = 0.0f;
    vert[2].v = 1.0f;
    vert[2].argb = 0xFFFFFFFF;
    vert[2].flags = PVR_CMD_VERTEX;

    vert[3].x = SCREEN_W;
    vert[3].y = SCREEN_H;
    vert[3].z = 1;
    vert[3].u = 1.0f;
    vert[3].v = 1.0f;
    vert[3].argb = 0xFFFFFFFF;
    vert[3].flags = PVR_CMD_VERTEX_EOL;

    pvr_prim(&vert, sizeof(vert));
}

// -------------------
// Convierte YUV420 planar a YUV422 interleaved para PVR
// -------------------
void yuv420_to_yuv422(plm_frame_t *frame, uint16_t *vram)
{
    int w = frame->width;
    int h = frame->height;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x += 2)
        {
            int y0 = frame->y.data[y * w + x];
            int y1 = frame->y.data[y * w + x + 1];

            // Upsample simple Cr/Cb 4:2:0
            int cr = frame->cr.data[(y / 2) * (w / 2) + x / 2];
            int cb = frame->cb.data[(y / 2) * (w / 2) + x / 2];

            vram[y * w + x + 0] = (y0 << 8) | cb; // Y0 Cb
            vram[y * w + x + 1] = (y1 << 8) | cr; // Y1 Cr
        }
    }
}