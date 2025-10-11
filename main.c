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
#include "video_player.h"
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include <stdint.h>
#include <kos/dbglog.h>

#define FONT_IMG_W 512
#define FONT_IMG_H 372
#define FONT_CHAR_W 16 // ancho fijo en tu atlas, ajustar si tu script usa otro
#define FONT_CHAR_H 16 // alto de cada carácter

int video_width = 0;
int video_height = 0;
int state = STATE_INTRO;
Menu main_menu;
int menu_active = 0;
int playing_video = 0;
int video_destroyed = 0;
plm_t *mpeg = NULL;
pvr_vertex_t vert[4];
pvr_ptr_t video_tex = 0;

/* Variables PVR */
pvr_poly_cxt_t cxt;
pvr_poly_hdr_t hdr;

pvr_ptr_t textBoxTex;
uint32 textBox_w, textBox_h;

Scene scenes[MAX_SCENES];
char buffer[256];
int menu_idle_time;

// Scene intro;

// Prototipos
void on_menu_fadeout_complete();
void print_memory_status();

uint32 bt_w, bt_h;         // tamaño real de la imagen
uint32 bt_tex_w, bt_tex_h; // tamaño PVR (potencia de 2)

pvr_ptr_t buttons_tex;

int main()
{
    // -------------------
    // Inicialización
    // -------------------
    maple_init();    // Controladores
    vid_set_mode(DM_640x480, PM_RGB565);
    cont_init();
    pvr_init_defaults();
    audio_init();
    font_init();
    buttons_tex = load_png_texture("/rd/botones2.png", &bt_w, &bt_h, &bt_tex_w, &bt_tex_h);
    menu_idle_time = 0;

    // Inicializar fade (valores seguros)
    g_fade.active = 0;
    g_fade.elapsed_ms = 0;
    g_fade.alpha = 0.0f;
    g_fade.duration_ms = 600; // fade de 600 ms (ajustalo a tu gusto)
    g_fade.on_complete = NULL;

    menu_active = 0; // Flag para mostrar el menú

    // --- Inicializar escenas ---
    scene_count = 3;
    for (int i = 0; i < scene_count; i++)
        scene_init(&scenes[i]);
    int ignore_input_until_release = 0;
    // scene_init(&intro);
    change_scene("/rd/intro.json");
    uint32_t prev_secs = 0, prev_ms = 0;

    // Reservar VRAM para textura de video
    video_tex = pvr_mem_malloc(VIDEO_W * VIDEO_H * 2);
    if (!video_tex)
    {
        dbglog(DBG_INFO,"No se pudo reservar VRAM para el video\n");
        return -1;
    }

    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                     PVR_TXRFMT_YUV422 | PVR_TXRFMT_NONTWIDDLED,
                     VIDEO_W, VIDEO_H, video_tex, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &cxt);

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
            menu_idle_time = 0;
            script_update(&scenes[current_scene], delta_ms, (pressed & CONT_A));

            /*if (s->text_lines && s->text_line_index < s->text_line_count)
            {
                const char *line = s->text_lines[s->text_line_index];

                // Máquina de escribir
                if (s->chars_displayed < strlen(line))
                {
                    s->chars_displayed++;
                }
            }*/
            // --- Render ---
            pvr_wait_ready();
            pvr_scene_begin();
            // --- Actualizar animaciones ---
            update_animations(&scenes[current_scene], delta_ms);
            update_transition(delta_ms);
            scene_render(&scenes[current_scene]);
            // snprintf(buffer, sizeof(buffer), "Time: %d", delta_ms);
            pvr_scene_finish();

            if (end_scene == 1)
            {
                end_scene = 0;
                playing_video = 1;
                state = STATE_VIDEO;
            }
            break;
        case STATE_VIDEO:
            music_volume = 128;
            play_video("/cd/intro.mpg", "/cd/video_audio.wav");
            break;

        case STATE_MENU:
            if (!video_destroyed)
            {
                plm_destroy(mpeg);
                pvr_mem_free(video_tex);
                video_destroyed = 1;
               dbglog(DBG_INFO,"Destruimos el video y liberamos memoria...\n");
            }
            g_fade.alpha = 0;
            // --- Detectar inactividad ---
            menu_idle_time += delta_ms;

            // Si hay input, reiniciamos el contador
            if (st && (st->buttons & (CONT_A | CONT_B | CONT_START | CONT_DPAD_UP | CONT_DPAD_DOWN | CONT_DPAD_LEFT | CONT_DPAD_RIGHT)))
            {
                menu_idle_time = 0;
            }

            // Si pasó 1 minuto sin input → volver a la intro
            if (menu_idle_time >= 60000)
            {
                dbglog(DBG_INFO,"Inactividad detectada. Volviendo a la intro...\n");
                menu_idle_time = 0;
                menu_active = 0;
                state = STATE_INTRO;
                change_scene("/rd/intro.json");
                audio_stop_music();
                break;
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
            fade_update(delta_ms);
            // Render del menú
            pvr_wait_ready();
            pvr_scene_begin();

            pvr_list_begin(PVR_LIST_TR_POLY);
            draw_block(&fondoMenu, 0xFF0000FF);
            menu_draw(&main_menu);
            pvr_list_finish();
            pvr_scene_finish();
            break;

        case STATE_GAME:

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
            fade_update(delta_ms);
            // --- Render ---
            pvr_wait_ready();
            pvr_scene_begin();
            // --- Actualizar animaciones ---
            update_animations(&scenes[current_scene], delta_ms);
            update_transition(delta_ms);
            scene_render(&scenes[current_scene]);
            pvr_scene_finish();
            if (pending_scene_change)
            {
                // copio el nombre a una variable local por si alguien más lo pisa
                char tmp_scene[NEXT_SCENE_NAME_SIZE];
                strncpy(tmp_scene, pending_scene_name, NEXT_SCENE_NAME_SIZE);
                tmp_scene[NEXT_SCENE_NAME_SIZE - 1] = '\0';

                // limpiar la marca antes de ejecutar para evitar reentradas
                pending_scene_change = 0;
                pending_scene_name[0] = '\0';

                dbglog(DBG_INFO,"[main] Ejecutando change_scene('%s')\n", tmp_scene);
                change_scene(tmp_scene);

                // opcional: resetear script, estados, etc. según lo que quieras al cargar escena
                script_reset();
                // current_scene ya lo pone change_scene()?
                // state = STATE_INTRO; // si querés entrar al modo INTRO inmediatamente
            }
            break;

        default:
            break;
        }
    }
    // Cleanup
    pvr_shutdown();
    audio_shutdown();
    return 0;
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