#include <kos.h>
#include <png/png.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <stdlib.h>
#include <string.h>
#include "font.h"
#include "sprite.h"
#include "scene.h"
#include "script.h"
#include "audio.h"
#include "menu.h"
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include "pl_mpeg.h"

#define FONT_IMG_W 512
#define FONT_IMG_H 372
#define FONT_CHAR_W 16 // ancho fijo en tu atlas, ajustar si tu script usa otro
#define FONT_CHAR_H 16 // alto de cada carácter

// --- Texturas ---
// pvr_ptr_t bg_tex;
// uint32 bg_w, bg_h;      // tamaño real de la imagen
// uint32 tex_w, tex_h;    // tamaño PVR (potencia de 2)
pvr_ptr_t textBoxTex;
uint32 textBox_w, textBox_h;

plm_t *mpeg;

Scene scenes[MAX_SCENES];
char buffer[256];

//static pvr_ptr_t video_tex = NULL;
// Definición de las variables globales declaradas en audio.h

// --- Video globals ---
//#define VIDEO_W 256 // o 640, depende de tu MPEG
//#define VIDEO_H 128 // ajustá a la resolución real

#define FRAME_WIDTH 256
#define FRAME_HEIGHT 128

static pvr_ptr_t video_frame = NULL;
static pvr_poly_cxt_t cxt;
static pvr_poly_hdr_t hdr;

plm_frame_t *frame = NULL;

// Vertex buffer para renderizar quad
// static pvr_vertex_t vert[4];

void render_video_frame(plm_frame_t *video_frame);

// Callbacks
// -----------------------------
/*void my_video_callback(plm_t *plm, plm_frame_t *frame, void *user)
{
    // Convertir a RGB565 y subir a VRAM
    uint16_t *rgb565 = (uint16_t *)malloc(VIDEO_W * VIDEO_H * 2);
    if (!rgb565)
        return;

    int src_w = frame->width;
    int src_h = frame->height;

    for (int y = 0; y < VIDEO_H; y++)
    {
        for (int x = 0; x < VIDEO_W; x++)
        {
            // Escalar de forma simple
            int src_x = x * src_w / VIDEO_W;
            int src_y = y * src_h / VIDEO_H;

            int Y = frame->y.data[src_y * src_w + src_x];
            int Cb = frame->cb.data[(src_y / 2) * (frame->cb.width) + (src_x / 2)];
            int Cr = frame->cr.data[(src_y / 2) * (frame->cr.width) + (src_x / 2)];

            int R = Y + 1.402f * (Cr - 128);
            int G = Y - 0.344136f * (Cb - 128) - 0.714136f * (Cr - 128);
            int B = Y + 1.772f * (Cb - 128);

            if (R < 0)
                R = 0;
            if (R > 255)
                R = 255;
            if (G < 0)
                G = 0;
            if (G > 255)
                G = 255;
            if (B < 0)
                B = 0;
            if (B > 255)
                B = 255;

            rgb565[y * VIDEO_W + x] = ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);
        }
    }

    // Subir a VRAM
    pvr_txr_load(rgb565, video_tex, VIDEO_W * VIDEO_H * 2);
    free(rgb565);
}*/

void my_audio_callback(plm_t *plm, plm_samples_t *samples, void *user)
{
    // samples->interleaved contiene el audio stereo
    // Acá llamá a tu sistema de audio o a wav_play_buffer()
    audio_play_music("/cd/video_audio.wav", 0);
}

/*void render_video_quad()
{
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                     PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED,
                     VIDEO_W, VIDEO_H, video_tex, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &cxt);

    // Quad en pantalla completa
    pvr_vertex_t vert[4];
    vert[0].x = 0;
    vert[0].y = 0;
    vert[0].z = 1;
    vert[0].u = 0;
    vert[0].v = 0;
    vert[0].argb = 0xFFFFFFFF;
    vert[1].x = 640;
    vert[1].y = 0;
    vert[1].z = 1;
    vert[1].u = 1;
    vert[1].v = 0;
    vert[1].argb = 0xFFFFFFFF;
    vert[2].x = 0;
    vert[2].y = 480;
    vert[2].z = 1;
    vert[2].u = 0;
    vert[2].v = 1;
    vert[2].argb = 0xFFFFFFFF;
    vert[3].x = 640;
    vert[3].y = 480;
    vert[3].z = 1;
    vert[3].u = 1;
    vert[3].v = 1;
    vert[3].argb = 0xFFFFFFFF;

    pvr_prim(vert, sizeof(vert));
}*/
//plm_frame_t frame;

audio_play_music("/cd/video_audio.wav", 0);

int main()
{
    vid_set_mode(DM_640x480, PM_RGB565);
    dbgio_init();
    cont_init();
    pvr_init_defaults();
    font_init();
    audio_init();

    // Inicializar fade (valores seguros)
    g_fade.active = 0;
    g_fade.elapsed_ms = 0;
    g_fade.alpha = 0.0f;
    g_fade.duration_ms = 600; // fade de 600 ms (ajustalo a tu gusto)
    g_fade.on_complete = NULL;

    // Reservamos textura en VRAM
    video_frame = pvr_mem_malloc(FRAME_WIDTH * FRAME_HEIGHT * 2);

    Menu main_menu;
    int menu_active = 0; // Flag para mostrar el menú

    // Inicializar video (ejemplo: 300 frames a 15 FPS + audio wav)
    // --- Inicializar escenas ---
    scene_count = 3;
    for (int i = 0; i < scene_count; i++)
        scene_init(&scenes[i]);
    int ignore_input_until_release = 0;

    uint32_t prev_secs = 0, prev_ms = 0;
    timer_ms_gettime(&prev_secs, &prev_ms);
    // uint32 prev_buttons = 0;
    //  Asumo que prev_secs/prev_ms están inicializados antes del loop:
    // uint32_t prev_secs = 0, prev_ms = 0; timer_ms_gettime(&prev_secs, &prev_ms);
    // Instalar callbacks
    //plm_set_video_decode_callback(mpeg, my_video_callback, NULL);
    

    // Loop principal
    // -----------------------------
    // Loop de reproducción
    // -----------------------------
    plm_t *mpeg;
    pvr_mem_reset();
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    pvr_scene_finish();

    mpeg = plm_create_with_filename("/cd/intro.mpg");
    if (!mpeg)
    {
        printf("Error opening video file\n");
        return -1;
    }
    plm_set_audio_enabled(mpeg, 0);

    int frame_width = plm_get_width(mpeg);
    int frame_height = plm_get_height(mpeg);
    printf("Video width: %d, height: %d\n", frame_width, frame_height);

    //if (frame_width != FRAME_WIDTH || frame_height != FRAME_HEIGHT)
    //{
      //  printf("Video frame size mismatch\n");
        //return -1;
    //}
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED,
                     FRAME_WIDTH, FRAME_HEIGHT, video_frame, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &cxt);

    uint8_t frame_num = 0;
    uint64_t st, end, delta;

    while (1)
    {
        st = timer_ms_gettime64();
        frame = plm_decode_video(mpeg);
        if (!frame)
        {
            printf("End of video\n");
            break;
        }
        frame_num++;

        // Load the Y component of the video frame
        pvr_txr_load((void *)frame->y.data, video_frame, FRAME_WIDTH * FRAME_HEIGHT * 2);

        pvr_wait_ready();
        pvr_scene_begin();
        pvr_set_bg_color(0.0f, 0.0f, 1.0f);

        // Set up the rendering context and matrix
        pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED,
                         FRAME_WIDTH, FRAME_HEIGHT, video_frame, PVR_FILTER_NONE);
        pvr_poly_compile(&hdr, &cxt);

        // Render the video frame
        render_video_frame(frame);

        pvr_scene_finish();

        end = timer_ms_gettime64();
        delta = end - st;
        printf("Frame: %d (%0.3f) took %llums\n", frame_num, frame->time, delta);
        fflush(stdout);

        /*MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st)
        if (st->buttons & CONT_START)
            //goto exit_loop;
        MAPLE_FOREACH_END()*/
    }
    // Loop principal del video
    /*while (playing_video || g_fade.active)
    {
        // --- Calcular delta ---
        uint32_t now_secs = 0, now_ms = 0;
        timer_ms_gettime(&now_secs, &now_ms);
        int delta_ms = (int)((now_secs - prev_secs) * 1000 + (now_ms - prev_ms));
        if (delta_ms < 0)
            delta_ms = 0;
        if (delta_ms > 200)
            delta_ms = 200;
        prev_secs = now_secs;
        prev_ms = now_ms;

        cont_state_t *st = (cont_state_t *)maple_dev_status(maple_enum_type(0, MAPLE_FUNC_CONTROLLER));
        uint32 pressed = st ? st->buttons : 0;

        // --- Actualizar video solo si no hay fade activo ---
        if (!g_fade.active || g_fade.fading_out)
        {
            video_update(&vp);

            // Chequear fin de video o salto
            if (vp.finished || (pressed & CONT_A))
            {
                ignore_input_until_release = 1;

                // Activar fade-out SOLO si no estaba activo
                if (!g_fade.active)
                {
                    g_fade.active = 1;
                    g_fade.alpha = 0.0f;
                    g_fade.duration_ms = 600; // fade-out del video
                    g_fade.elapsed_ms = 0;
                    g_fade.fading_out = 1;
                    g_fade.fading_in = 0;

                    dbgio_printf("Fade-out video start\n");
                }
            }
        }

        // --- Actualizar fade ---
        if (g_fade.active)
        {
            int add = (delta_ms > 33) ? 33 : delta_ms;
            g_fade.elapsed_ms += add;
            float t = (float)g_fade.elapsed_ms / g_fade.duration_ms;
            if (t > 1.0f)
                t = 1.0f;

            if (g_fade.fading_out)
                g_fade.alpha = t; // video se oscurece
            else if (g_fade.fading_in)
                g_fade.alpha = 1.0f - t; // menú se aclara

            if (t >= 1.0f)
            {
                if (g_fade.fading_out)
                {
                    // Fade-out del video completo
                    g_fade.active = 0;
                    g_fade.fading_out = 0;

                    // Detener video y audio
                    // video_shutdown(&vp);
                    audio_stop_music();

                    // Preparar menú
                    menu_active = 1;
                    menu_init(&main_menu);

                    // Iniciar fade-in del menú
                    g_fade.active = 1;
                    g_fade.alpha = 1.0f; // empieza negro
                    g_fade.duration_ms = 600;
                    g_fade.elapsed_ms = 0;
                    g_fade.fading_in = 1;
                }
                else if (g_fade.fading_in)
                {
                    // Fade-in del menú completo
                    g_fade.active = 0;
                    g_fade.fading_in = 0;
                    playing_video = 0;
                    // Música del menú puede empezar aquí si no empezó en menu_init
                    // audio_play_menu_music();
                }
            }
        }

        // --- Render ---
        pvr_wait_ready();
        pvr_scene_begin();

        // Video o pantalla negra
        pvr_list_begin(PVR_LIST_OP_POLY);
        if (!g_fade.fading_in) // no dibujar video durante fade-in del menú
            video_draw(&vp);
        pvr_list_finish();

        // Overlay de fade encima
        pvr_list_begin(PVR_LIST_TR_POLY);
        if (g_fade.alpha > 0.0f)
            draw_fade_overlay(g_fade.alpha);
        pvr_list_finish();

        pvr_scene_finish();
    }*/

    while (menu_active || g_fade.active)
    {
        cont_state_t *st = (cont_state_t *)maple_dev_status(maple_enum_type(0, MAPLE_FUNC_CONTROLLER));

        // Solo procesar input si el menú sigue activo
        if (st && menu_active)
        {
            if (ignore_input_until_release)
            {
                // esperamos hasta que se suelte
                if (!(st->buttons & CONT_A))
                {
                    ignore_input_until_release = 0; // ahora sí podemos aceptar input
                }
            }
            else
            {
                menu_update(&main_menu, st, &menu_active);
            }
            // menu_update(&main_menu, st, &menu_active);
        }

        // --- tiempo ---
        uint32_t now_secs = 0, now_ms = 0;
        timer_ms_gettime(&now_secs, &now_ms);

        int delta_ms = (int)((now_secs - prev_secs) * 1000 + (now_ms - prev_ms));

        // protección contra valores raros
        if (delta_ms < 0)
            delta_ms = 0;
        if (delta_ms > 200)
            delta_ms = 200; // si hay lag, limitamos para evitar saltos instantáneos

        // actualizar prev inmediatamente
        prev_secs = now_secs;
        prev_ms = now_ms;

        // --- Actualizar fade (si está activo) ---
        if (g_fade.active)
        {
            g_fade.elapsed_ms += delta_ms;
            float t = (float)g_fade.elapsed_ms / (float)g_fade.duration_ms;
            if (t > 1.0f)
                t = 1.0f;
            g_fade.alpha = t;

            if (g_fade.alpha >= 1.0f)
            {
                // llegamos al negro completo: desactivamos y ejecutamos callback
                g_fade.alpha = 1.0f;
                g_fade.active = 0;

                // **IMPORTANTE**: el callback no debería llamar a pvr_prim / pvr_list_*.
                if (g_fade.on_complete)
                    g_fade.on_complete();

                // ahora sí: señalamos que el menú puede cerrarse
                menu_active = 0;
            }
        }
        dbgio_printf("delta=%d alpha=%.3f active=%d\n", delta_ms, g_fade.alpha, g_fade.active);
        // --- Render del menú ---
        pvr_wait_ready();
        pvr_scene_begin();

        pvr_list_begin(PVR_LIST_TR_POLY);
        draw_block(&fondoMenu, 0xFF0000FF);
        menu_draw(&main_menu);
        // --- Fade overlay encima, en su propia lista ---
        if (g_fade.alpha > 0.0f)
        {
            draw_fade_overlay(g_fade.alpha); // ver función abajo
        }
        pvr_list_finish();
        pvr_scene_finish();
    }

    while (1)
    {
        uint32_t now_secs = 0, now_ms = 0;
        timer_ms_gettime(&now_secs, &now_ms);
        // Calcular delta en milisegundos
        int delta_ms = (int)((now_secs - prev_secs) * 1000 + (now_ms - prev_ms));
        prev_secs = now_secs;
        prev_ms = now_ms;
        cont_state_t *st = (cont_state_t *)maple_dev_status(maple_enum_type(0, MAPLE_FUNC_CONTROLLER));
        uint32 pressed = st ? st->buttons : 0;
        script_update(&scenes[current_scene], delta_ms, (pressed & CONT_A));
        Scene *s = &scenes[current_scene];

        if (s->text_lines && s->text_line_index < s->text_line_count)
        {
            const char *line = s->text_lines[s->text_line_index];

            // Máquina de escribir
            if (s->chars_displayed < strlen(line))
            {
                s->chars_displayed++;
            }
        }

        // prev_buttons = pressed;

        // --- Render ---
        pvr_wait_ready();
        pvr_scene_begin();
        // --- Actualizar animaciones ---
        // script_update(s, delta_ms);
        update_animations(&scenes[current_scene], delta_ms);
        update_transition(delta_ms);
        // update_music();
        scene_render(&scenes[current_scene]);
        // snprintf(buffer, sizeof(buffer), "Time: %d", delta_ms);
        // draw_string(20.f, 310.0f, buffer, strlen(buffer), NULL, 1.0f);
        pvr_scene_finish();
    }

    // pvr_mem_free(bg_tex);
    pvr_mem_free(textBoxTex);
    plm_destroy(mpeg);
    pvr_mem_free(video_frame);
    pvr_shutdown();
    audio_shutdown();
    return 0;
}
