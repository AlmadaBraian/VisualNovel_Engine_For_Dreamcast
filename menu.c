#include "menu.h"
#include <stdlib.h>
#include "font.h"
#include "scene.h"
#include "script.h"
#include "audio.h"
#include "sprite.h"
#include "video_player.h"

Block fondoMenu = { 0.0f, 0.0f, 640.0f, 480.0f };


void menu_init(Menu *menu) {
    menu->selected = 0;
    menu->option_count = MENU_COUNT;
    menu->option_text[MENU_NEW_GAME]  = "Nueva Partida";
    menu->option_text[MENU_LOAD_GAME] = "Cargar Partida";
    menu->option_text[MENU_SAVE_GAME] = "Guardar Partida";
    menu->option_text[MENU_EXIT]      = "Salir";
	music_volume = 20;
	audio_play_music("/cd/music/musica_2.wav", 1);
}

void menu_draw(Menu *menu) {
    for (int i = 0; i < menu->option_count; i++) {
        float x = 100;
        float y = 100 + i * 40;
        const char *prefix = (i == menu->selected) ? "> " : "  ";
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%s%s", prefix, menu->option_text[i]);
        draw_string(x, y, buffer, 20, NULL,1.0f);  // tu función de draw_string
    }
    draw_transition();
}

void start_new_game_callback(void) {
    //current_scene = 0;
    end_scene = 0;
    change_scene("/rd/escena.json");
    state = STATE_GAME;
    draw_text_box = true;

}

// --- Fade to black y carga de nueva partida ---
void start_new_game_with_fade(void (*callback)(void)) {
    g_fade.active = 1;
    g_fade.alpha = 0.0f;
    g_fade.duration_ms = 1000;
    g_fade.elapsed_ms = 0;
    g_fade.on_complete = callback;
}


void load_game(void) {
    // Aquí vas a cargar datos de un save
}

void save_game(void) {
    // Aquí vas a guardar datos de la partida
}

void menu_update(Menu *menu, cont_state_t *st, int *menu_active) {
    static int prev_up = 0, prev_down = 0, prev_a = 0;

    // Edge detection para teclas
    int up_pressed   = st->buttons & CONT_DPAD_UP;
    int down_pressed = st->buttons & CONT_DPAD_DOWN;
    int a_pressed    = st->buttons & CONT_A;
	
    if (up_pressed && !prev_up) {
		audio_play_sound("move_cursor");
        menu->selected--;
        if (menu->selected < 0) menu->selected = menu->option_count - 1;
    }
    if (down_pressed && !prev_down) {
		audio_play_sound("move_cursor");
        menu->selected++;
        if (menu->selected >= menu->option_count) menu->selected = 0;
    }

    if (a_pressed && !prev_a) {
		audio_play_sound("acept");;
        switch (menu->selected) {
			case MENU_NEW_GAME:
			//audio_stop_music();
			start_new_game_with_fade(start_new_game_callback);
			*menu_active = 0;
			break;
            case MENU_LOAD_GAME:
                load_game();
                *menu_active = 0;
                break;
            case MENU_SAVE_GAME:
                save_game();
                break;
            case MENU_EXIT:
                *menu_active = 0;
                exit(0);
                break;
			default: break;
        }
    }

    prev_up = up_pressed;
    prev_down = down_pressed;
    prev_a = a_pressed;
}
