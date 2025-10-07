#include <kos.h>
#include <png/png.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "font.h"
#include "sprite.h"
#include "scene.h"
#include "script.h"
#include "audio.h"
#include "menu.h"
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#define PL_MPEG_IMPLEMENTATION
#include "mpegDC.h"

#define VIDEO_W 256
#define VIDEO_H 128
#define SCREEN_W 640
#define SCREEN_H 480
#define UV_EPSILON 0.001f

#define FONT_IMG_W 512
#define FONT_IMG_H 372
#define FONT_CHAR_W 16 // ancho fijo en tu atlas, ajustar si tu script usa otro
#define FONT_CHAR_H 16 // alto de cada carácter

pvr_ptr_t textBoxTex;
uint32 textBox_w, textBox_h;

int video_width;
int video_height;

Scene scenes[MAX_SCENES];
char buffer[256];

Scene intro;

// Variables globales PVR
pvr_poly_cxt_t cxt;
pvr_poly_hdr_t hdr;
pvr_vertex_t vert[4];
pvr_ptr_t video_tex;

// Prototipos
void render_video_frame();
void yuv420_to_yuv422(plm_frame_t *frame, uint16_t *vram);
void on_video_fadeout_complete();
void on_menu_fadeout_complete();
void print_memory_status();

uint32 bt_w, bt_h;         // tamaño real de la imagen
uint32 bt_tex_w, bt_tex_h; // tamaño PVR (potencia de 2)

void fade_update(int delta_ms)
{
    if (!g_fade.active)
        return;
    printf("Activando el fade\n");
    g_fade.elapsed_ms += delta_ms;
    float t = (float)g_fade.elapsed_ms / (float)g_fade.duration_ms;
    if (t > 1.0f)
        t = 1.0f;

    if (g_fade.fading_out)
    {
        printf("Fade-out: se oscurece\n");
        g_fade.alpha = t; // Fade-out: se oscurece
    }
    else if (g_fade.fading_in)
    {
        printf("Fade-in: se aclara\n");
        g_fade.alpha = 1.0f - t; // Fade-in: se aclara
    }

    if (t >= 1.0f)
    {
        printf("t >= 1.0f\n");
        g_fade.active = 0;
        if (g_fade.on_complete)
        {
            printf("Fade completado -> callback\n");
            g_fade.on_complete();
            g_fade.on_complete = NULL;
        }
        g_fade.fading_out = 0;
        g_fade.fading_in = 0;
    }
}
int video_destroyed;
int menu_active;

Menu main_menu;
plm_t *mpeg;
pvr_ptr_t buttons_tex;
int playing_video;
int state = STATE_INTRO;

void play_intro_video()
{
    uint32_t prev_secs = 0, prev_ms = 0;
    // --- Cargar video ---
    mpeg = plm_create_with_filename("/cd/intro.mpg");
    if (!mpeg)
    {
        printf("No se pudo abrir intro.mpg\n");
        return;
    }
    plm_set_audio_enabled(mpeg, 0); // Audio externo

    // Reproducir WAV externo
    audio_play_music("/cd/video_audio.wav", 0);

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
    // detén audio del video si está sonando
    //audio_stop_music();
}

int main()
{
    // -------------------
    // Inicialización
    // -------------------
    vid_set_mode(DM_640x480, PM_RGB565);
    dbgio_init();
    cont_init();
    pvr_init_defaults();
    audio_init();
    font_init();
    playing_video = 1;
    buttons_tex = load_png_texture("/rd/botones2.png", &bt_w, &bt_h, &bt_tex_w, &bt_tex_h);

    // Inicializar fade (valores seguros)
    g_fade.active = 0;
    g_fade.elapsed_ms = 0;
    g_fade.alpha = 0.0f;
    g_fade.duration_ms = 600; // fade de 600 ms (ajustalo a tu gusto)
    g_fade.on_complete = NULL;

    menu_active = 0; // Flag para mostrar el menú

    // Inicializar video (ejemplo: 300 frames a 15 FPS + audio wav)
    // --- Inicializar escenas ---
    scene_count = 3;
    for (int i = 0; i < scene_count; i++)
        scene_init(&scenes[i]);
    int ignore_input_until_release = 0;
    scene_init(&intro);
    change_scene("/rd/intro.json");
    uint32_t prev_secs = 0, prev_ms = 0;
    // uint32 prev_buttons = 0;

    // Reservar VRAM para textura de video
    video_tex = pvr_mem_malloc(VIDEO_W * VIDEO_H * 2);
    if (!video_tex)
    {
        printf("No se pudo reservar VRAM para el video\n");
        return -1;
    }

    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                     PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED,
                     VIDEO_W, VIDEO_H, video_tex, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &cxt);

    // Cargar video
    /*mpeg = plm_create_with_filename("/cd/intro.mpg");
    if (!mpeg)
    {
        printf("No se pudo abrir intro.mpg\n");
        return -1;
    }
    plm_set_audio_enabled(mpeg, 0); // Audio externo

    // Reproducir WAV externo
    audio_play_music("/cd/video_audio.wav", 0);

    printf("Video size: %dx%d\n", plm_get_width(mpeg), plm_get_height(mpeg));

    video_width = plm_get_width(mpeg);
    video_height = plm_get_height(mpeg);

    printf("Video real: %dx%d, textura usada: %dx%d\n",
           video_width, video_height, VIDEO_W, VIDEO_H);

    uint64_t video_start_ms = timer_ms_gettime64();*/
    // -------------------
    // Loop principal
    // -------------------
    // state = STATE_VIDEO;
    plm_frame_t *frame;
    uint8_t frame_num = 0;

    // double last_frame_time = 0.0;
    // timer_ms_gettime(&prev_secs, &prev_ms);

    while (1)
    {

        cont_state_t *st = (cont_state_t *)maple_dev_status(maple_enum_type(0, MAPLE_FUNC_CONTROLLER));
        uint32 pressed = st ? st->buttons : 0;

        // Obtener tiempo actual
        uint32_t now_secs = 0, now_ms = 0;
        timer_ms_gettime(&now_secs, &now_ms);
        int delta_ms = (int)((now_secs - prev_secs) * 1000 + (now_ms - prev_ms));
        if (delta_ms < 0)
            delta_ms = 0;
        if (delta_ms > 200)
            delta_ms = 200;
        prev_secs = now_secs;
        prev_ms = now_ms;
        Scene *s = &scenes[current_scene];
        /* code */
        switch (state)
        {

        case STATE_INTRO:
            fade_update(delta_ms);
            // uint32_t now_secs = 0, now_ms = 0;
            // timer_ms_gettime(&now_secs, &now_ms);
            //  Calcular delta en milisegundos
            // printf("New game\n");
            // cont_state_t *st = (cont_state_t *)maple_dev_status(maple_enum_type(0, MAPLE_FUNC_CONTROLLER));
            script_update(&scenes[current_scene], delta_ms, (pressed & CONT_A));

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

            if (end_scene == 1)
            {
                end_scene = 0;
                state = STATE_VIDEO;
            }
            break;
        case STATE_VIDEO:
            play_intro_video();
            break;

        case STATE_MENU:

            // actualizar fade de forma centralizada
            fade_update(delta_ms);

            if (!video_destroyed)
            {
                plm_destroy(mpeg);
                pvr_mem_free(video_tex);
                video_destroyed = 1;
                g_fade.alpha = 0;
            }

            // Solo procesar input si el menú sigue activo
            if (st && menu_active)
            {
                if (ignore_input_until_release)
                {
                    if (!(st->buttons & CONT_A))
                        ignore_input_until_release = 0;
                }
                else
                {
                    menu_update(&main_menu, st, &menu_active);
                }
            }

            // dbgio_printf("delta=%d alpha=%.3f active=%d\n", /* delta_ms */ 0, g_fade.alpha, g_fade.active);

            // Render del menú
            pvr_wait_ready();
            pvr_scene_begin();

            pvr_list_begin(PVR_LIST_TR_POLY);
            draw_block(&fondoMenu, 0xFF0000FF);
            menu_draw(&main_menu);
            if (g_fade.alpha > 0.0f)
                draw_fade_overlay(g_fade.alpha);
            pvr_list_finish();
            pvr_scene_finish();
            break;

        case STATE_GAME:
            fade_update(delta_ms);
            // uint32_t now_secs = 0, now_ms = 0;
            // timer_ms_gettime(&now_secs, &now_ms);
            //  Calcular delta en milisegundos
            // printf("New game\n");
            // cont_state_t *st = (cont_state_t *)maple_dev_status(maple_enum_type(0, MAPLE_FUNC_CONTROLLER));
            // uint32 pressed = st ? st->buttons : 0;
            script_update(&scenes[current_scene], delta_ms, (pressed & CONT_A));
            // Scene *s = &scenes[current_scene];

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
            break;

        default:
            break;
        }
        // pvr_scene_finish();
        // print_memory_status();
    }
    // Cleanup
    pvr_shutdown();
    audio_shutdown();
    return 0;
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

void on_video_fadeout_complete()
{
    printf("on_video_fadeout_complete\n");
    state = STATE_MENU; // Cambiar al menú
    menu_active = 1;
    menu_init(&main_menu);
    playing_video = 0;
    // state = STATE_MENU;
    //  No destruimos video ni textura aquí: lo hacemos en el loop, una vez que el menú empieza a renderizar
    //  audio_stop_music();
}
void print_memory_status()
{
    struct mallinfo mi = mallinfo();
    size_t free_ram = mi.fordblks;
    size_t used_ram = mi.uordblks;
    size_t total_ram = free_ram + used_ram;
    size_t free_vram = pvr_mem_available();

    printf("---- ESTADO DE MEMORIA ----\n");
    printf("RAM total : %u KB\n", total_ram / 1024);
    printf("RAM usada : %u KB\n", used_ram / 1024);
    printf("RAM libre : %u KB\n", free_ram / 1024);
    printf("VRAM libre: %u KB\n", free_vram / 1024);
    printf("----------------------------\n");
}