#include <kos.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h" // Usa cJSON para parsear el JSON
#include "scene.h"
#include "script.h"
#include "audio.h"

Script current_script = {0};
bool showButton = false;
char next_scene_name[NEXT_SCENE_NAME_SIZE];
int end_scene = 0;
char pending_scene_name[NEXT_SCENE_NAME_SIZE] = {0};
int pending_scene_change = 0;

// --- Helpers para mapear strings a ActionType ---
ActionType parse_action_type(const char *str)
{
    if (strcmp(str, "show_sprite") == 0)
        return ACTION_SHOW_SPRITE;
    if (strcmp(str, "off_sprite") == 0)
        return ACTION_OFF_SPRITE;
    if (strcmp(str, "play_sound") == 0)
        return ACTION_PLAY_SOUND;
    if (strcmp(str, "play_sound_L") == 0)
        return ACTION_PLAY_SOUND_L;
    if (strcmp(str, "animate") == 0)
        return ACTION_ANIMATE;
    if (strcmp(str, "show_text") == 0)
        return ACTION_SHOW_TEXT;
    if (strcmp(str, "play_music") == 0)
        return ACTION_PLAY_MUSIC;
    if (strcmp(str, "stop_music") == 0)
        return ACTION_STOP_MUSIC;
    if (strcmp(str, "next_scene") == 0)
        return ACTION_NEXT_SCENE;
    if (strcmp(str, "space") == 0)
        return ACTION_SPACE;
    if (strcmp(str, "end") == 0)
        return ACTION_END;

    return ACTION_SHOW_TEXT;
}

void change_scene_callback(void)
{
    // No ejecutar change_scene() acá (unsafe). Solo marcar la escena pendiente.
    strncpy(pending_scene_name, next_scene_name, NEXT_SCENE_NAME_SIZE - 1);
    pending_scene_name[NEXT_SCENE_NAME_SIZE - 1] = '\0';
    pending_scene_change = 1;
    printf("[DEBUG] Cambio de escena programado: %s\n", pending_scene_name);
}

// Esta función inicia un "movimiento" del sprite
void start_animation(Scene *scene, const char *sprite_name, const char *anim_name, int final_x, int final_y, int delta_ms)
{
    for (int i = 0; i < scene->sprite_count; i++)
    {
        Sprite *s = &scene->sprites[i];
        if (strcmp(s->name, sprite_name) == 0)
        {

            if (strcmp(anim_name, "caminar_derecha") == 0)
            {
                s->target_x = s->x + 100; // mover 100px a la derecha
                s->target_y = s->y;
                s->speed_x = 2.0f; // px por frame
                s->speed_y = 0;
                s->animating = 1;
            }
            else if (strcmp(anim_name, "caminar_izquierda") == 0)
            {
                s->target_x = s->x - 100;
                s->target_y = s->y;
                s->speed_x = 2.0f;
                s->speed_y = 0;
                s->animating = 1;
            }
            else if (strcmp(anim_name, "entrar_izquierda") == 0)
            {
                s->x = -s->width;
                s->target_x = final_x;
                s->target_y = s->y;
                s->speed_x = 2.0f;
                s->speed_y = 0;
            }
            else if (strcmp(anim_name, "entrar_derecha") == 0)
            {
                s->x = 640;
                s->target_x = final_x;
                s->target_y = s->y;
                s->speed_x = 2.0f; // px por frame
                s->speed_y = 0;
            }
            else if (strcmp(anim_name, "salir_izquierda") == 0)
            {
                s->target_x = -s->width;
                s->target_y = s->y;
                s->speed_x = 2.0f; // px por frame
                s->speed_y = 0;
                s->visible = 0;
            }
            else if (strcmp(anim_name, "salir_derecha") == 0)
            {
                s->target_x = 640;
                s->target_y = s->y;
                s->speed_x = 2.0f; // px por frame
                s->speed_y = 0;
                s->visible = 0;
            }
            else if (strcmp(anim_name, "subir") == 0)
            {
                s->target_x = s->x;
                s->target_y = s->y - 50;
                s->speed_x = 0; // px por frame
                s->speed_y = 2.0f;
            }
            else if (strcmp(anim_name, "bajar") == 0)
            {
                s->target_x = s->x;
                s->target_y = s->y + 50;
                s->speed_x = 0; // px por frame
                s->speed_y = 2.0f;
            }
            else if (strcmp(anim_name, "quedarse") == 0)
            {
                s->target_x = s->x;
                s->target_y = s->y;
                s->speed_x = 0; // px por frame
                s->speed_y = 0.0f;
            }
            else if (strcmp(anim_name, "fade_out") == 0)
            {
                s->fading_out = 1;
                // s->alpha_speed = s->alpha / 120.0f; // si querés que dure ~1s
                s->alpha_speed = s->alpha / (2000 / (float)delta_ms);
            }
            else if (strcmp(anim_name, "fade_in") == 0)
            {
                s->alpha = 0.0f;                // arranca invisible
                s->alpha_speed = 1.0f / 120.0f; // cuánto aumenta por ms
                s->fading_in = 1;               // activa el flag
                s->visible = 1;                 // debe ser visible para poder ir apareciendo
                s->x = final_x;
                s->y = final_y;
            }
            return;
        }
    }
}

// Llamar cada frame para actualizar posiciones
void update_animations(Scene *scene, int delta_ms)
{
    for (int i = 0; i < scene->sprite_count; i++)
    {
        Sprite *s = &scene->sprites[i];

        if (s->x < s->target_x)
        {
            s->x += s->speed_x * delta_ms;
            if (s->x > s->target_x)
                s->x = s->target_x;
        }
        else if (s->x > s->target_x)
        {
            s->x -= s->speed_x * delta_ms;
            if (s->x < s->target_x)
                s->x = s->target_x;
        }

        if (s->y < s->target_y)
        {
            s->y += s->speed_y * delta_ms;
            if (s->y > s->target_y)
                s->y = s->target_y;
        }
        else if (s->y > s->target_y)
        {
            s->y -= s->speed_y * delta_ms;
            if (s->y < s->target_y)
                s->y = s->target_y;
        }
        if (s->fading_out)
        {
            s->alpha -= s->alpha_speed * delta_ms;
            if (s->alpha <= 0.0f)
            {
                s->alpha = 0.0f;
                s->visible = 0;
                s->fading_out = 0;
            }
        }
        if (s->fading_in)
        {
            s->alpha += s->alpha_speed * delta_ms;
            if (s->alpha >= 1.0f)
            {
                s->alpha = 1.0f;
                s->fading_in = 0;
            }
        }
    }
}

// --- Carga del script desde JSON ---
void load_script(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        perror("Error");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *data = malloc(size + 1);
    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(data);
    free(data);
    if (!json)
    {
        printf("Error parsing JSON\n");
        return;
    }

    cJSON *script = cJSON_GetObjectItem(json, "script");
    int count = cJSON_GetArraySize(script);
    current_script.actions = calloc(count, sizeof(ScriptAction));
    current_script.count = count;
    current_script.current = 0;
    current_script.elapsed_ms = 0;

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(script, i);
        ScriptAction *act = &current_script.actions[i];

        cJSON *time = cJSON_GetObjectItem(item, "time");
        if (cJSON_IsNumber(time))
            act->time_ms = time->valueint;
        else
            act->time_ms = 0;

        act->type = parse_action_type(cJSON_GetObjectItem(item, "action")->valuestring);

        cJSON *sprite = cJSON_GetObjectItem(item, "sprite");
        if (sprite)
        {
            strncpy(act->sprite, sprite->valuestring, sizeof(act->sprite) - 1);
            act->sprite[sizeof(act->sprite) - 1] = '\0';
        }

        cJSON *anim = cJSON_GetObjectItem(item, "anim");
        if (anim)
        {
            strncpy(act->anim, anim->valuestring, sizeof(act->anim) - 1);
            act->anim[sizeof(act->anim) - 1] = '\0';
        }

        cJSON *sound = cJSON_GetObjectItem(item, "sound");
        if (sound)
        {
            strncpy(act->sound, sound->valuestring, sizeof(act->sound) - 1);
            act->sound[sizeof(act->sound) - 1] = '\0';
        }

        cJSON *music = cJSON_GetObjectItem(item, "music");
        if (music)
        {
            strncpy(act->music, music->valuestring, sizeof(act->music) - 1);
            act->music[sizeof(act->music) - 1] = '\0';
        }

        cJSON *wait_input = cJSON_GetObjectItem(item, "wait_input");
        if (wait_input)
        {
            act->wait_input = cJSON_IsTrue(wait_input); // devuelve 1 si es true, 0 si es false
        }

        cJSON *scene_item = cJSON_GetObjectItem(item, "scene");
        if (scene_item)
        {
            strncpy(act->scene, scene_item->valuestring, sizeof(act->scene) - 1);
            act->scene[sizeof(act->scene) - 1] = '\0';
        }

        cJSON *scene_new = cJSON_GetObjectItem(item, "scene_new");
        if (scene_new)
        {
            strncpy(act->scene_new, scene_new->valuestring, sizeof(act->scene_new) - 1);
            act->scene_new[sizeof(act->scene_new) - 1] = '\0';
        }

        cJSON *text = cJSON_GetObjectItem(item, "text");
        if (text && text->type == cJSON_Array)
        {
            int n = cJSON_GetArraySize(text);
            if (n > MAX_TEXT_LINES)
                n = MAX_TEXT_LINES;
            act->text_line_count = n;
            for (int j = 0; j < n; j++)
            {
                cJSON *line = cJSON_GetArrayItem(text, j);
                act->text_lines[j] = strdup(line->valuestring);
            }
        }
        else if (text && text->type == cJSON_String)
        {
            act->text_line_count = 1;
            act->text_lines[0] = strdup(text->valuestring);
        }
        else
        {
            act->text_line_count = 0;
        }

        cJSON *x = cJSON_GetObjectItem(item, "x");
        if (x)
            act->x = x->valueint;

        cJSON *y = cJSON_GetObjectItem(item, "y");
        if (y)
            act->y = y->valueint;

        cJSON *loop = cJSON_GetObjectItem(item, "loop");
        if (loop)
            act->loop = loop->valueint;

        cJSON *volume = cJSON_GetObjectItem(item, "volume");
        if (cJSON_IsNumber(volume))
        {
            int v = volume->valueint;
            if (v < 0)
                v = 0;
            if (v > 255)
                v = 255;
            act->volume = (uint8_t)v;
        }
    }

    cJSON_Delete(json);
}

static int last_button_state = 0;
void script_update(Scene *scene, int delta_ms, int button_pressed)
{
    // current_script.elapsed_ms += delta_ms;

    int button_pressed_edge = (button_pressed && !last_button_state);
    last_button_state = button_pressed;

    if (current_script.current >= current_script.count)
        return;

    ScriptAction *act = &current_script.actions[current_script.current];

    // Si no hay acción activa, activamos la siguiente
    if (!current_script.action_active)
    {
        current_script.action_active = 1;
        current_script.action_timer_ms = 0;

        switch (act->type)
        {
        case ACTION_SHOW_SPRITE:
            for (int i = 0; i < scene->sprite_count; i++)
            {
                if (strcmp(scene->sprites[i].name, act->sprite) == 0)
                {
                    scene->sprites[i].visible = 1;
                    scene->sprites[i].x = (float)act->x;
                    scene->sprites[i].y = (float)act->y;
                    scene->sprites[i].target_x = scene->sprites[i].x;
                    scene->sprites[i].target_y = scene->sprites[i].y;
                }
            }
            break;

        case ACTION_OFF_SPRITE:
            for (int i = 0; i < scene->sprite_count; i++)
            {
                if (strcmp(scene->sprites[i].name, act->sprite) == 0)
                {
                    scene->sprites[i].visible = 0;
                }
            }
            break;

        case ACTION_ANIMATE:
            start_animation(scene, act->sprite, act->anim, act->x, act->y, delta_ms);
            break;

        case ACTION_SHOW_TEXT:
            // Limpiar texto previo
            if (scene->text_lines)
            {
                for (int i = 0; i < scene->text_line_count; i++)
                    free(scene->text_lines[i]);
                free(scene->text_lines);
            }
            scene->text_line_count = act->text_line_count;
            scene->text_lines = malloc(sizeof(char *) * act->text_line_count);
            for (int i = 0; i < act->text_line_count; i++)
                scene->text_lines[i] = strdup(act->text_lines[i]);

            scene->text_line_index = 0;
            scene->chars_displayed = 0;
            scene->line_complete = 0;
            scene->just_started_text = 1;
            break;

        case ACTION_PLAY_SOUND:
            // audio_play_large_sound(act->sound, act->loop);
            audio_play_sound(act->sound);
            break;
        case ACTION_PLAY_SOUND_L:
            sound_volume = act->volume;
            audio_play_large_sound(act->sound, act->loop);
            // audio_play_sound("/rd/Ruido_subte.wav");
            break;

        case ACTION_PLAY_MUSIC:
            printf("[SCRIPT] Cambio de música a: %s\n", act->music);
            audio_stop_music();
            music_volume = act->volume;
            audio_play_music(act->music, act->loop);
            // No incrementes current acá. Dejemos que avance en el siguiente frame.
            break;
        case ACTION_STOP_MUSIC:
            printf("[SCRIPT] Parando musica en ACTION_STOP_MUSIC");
            audio_stop_music();
            // No incrementes current acá. Dejemos que avance en el siguiente frame.
            break;

        case ACTION_NEXT_SCENE:
            // if (!act->wait_input || button_pressed_edge) {
            strncpy(next_scene_name, act->scene_new, sizeof(next_scene_name));
            next_scene_name[sizeof(next_scene_name) - 1] = '\0'; // seguridad
            start_scene_transition(change_scene_callback, 1000); // 1 segundo
                                                                 // printf("[SCRIPT] Parando musica en cambio de escena");
            // audio_stop_music();
            // change_scene(act->scene_new);
            // script_reset();
            // return; // Salimos para no procesar más de esta acción
            //}
            break;
        case ACTION_SPACE:

            break;
            case ACTION_END:
                //scene_clear(scene);
                end_scene = 1;
            break;
        }
    }

    // Actualizamos temporizador de acción activa
    if (current_script.action_active)
    {
        current_script.action_timer_ms += delta_ms;
    }

    // Manejo por tipo de acción después de activarla
    switch (act->type)
    {
    case ACTION_SHOW_TEXT:
    {
        const char *current_line = scene->text_lines[scene->text_line_index];

        if (scene->just_started_text)
        {
            scene->just_started_text = 0;
            button_pressed_edge = 0;
        }

        if (scene->chars_displayed >= strlen(current_line))
        {
            scene->line_complete = 1;
        }

        if (scene->line_complete)
        {
            showButton = true;
            // --- Modo 1: esperar input ---
            if (act->wait_input && button_pressed_edge)
            {
                if (scene->text_line_index < scene->text_line_count - 1)
                {
                    scene->text_line_index++;
                    scene->chars_displayed = 0;
                    scene->line_complete = 0;
                    scene->just_started_text = 1;
                }
                else
                {
                    current_script.current++;
                    current_script.action_active = 0;
                    showButton = false;
                }
            }

            // --- Modo 2: autoavance ---
            else if (!act->wait_input && act->time_ms > 0 && current_script.action_timer_ms >= act->time_ms)
            {
                current_script.current++;
                current_script.action_active = 0;
                showButton = false;
            }
        }

        break;
    }

    case ACTION_ANIMATE:
        if (act->time_ms > 0)
        {
            // Esperar hasta que pase el tiempo indicado
            if (current_script.action_timer_ms >= act->time_ms)
            {
                current_script.current++;
                current_script.action_active = 0;
            }
        }
        else
        {
            // Si no tiene tiempo definido, avanzar enseguida
            current_script.current++;
            current_script.action_active = 0;
        }
        break;
    case ACTION_SHOW_SPRITE:
        if (!act->wait_input || button_pressed_edge || act->time_ms > 0)
        {
            current_script.current++;
            current_script.action_active = 0;
        }
        break;

    case ACTION_PLAY_SOUND:
        if (act->time_ms > 0)
        {
            // Esperar hasta que pase el tiempo indicado
            if (current_script.action_timer_ms >= act->time_ms)
            {
                current_script.current++;
                current_script.action_active = 0;
            }
        }
        else
        {
            // Si no tiene tiempo definido, avanzar enseguida
            current_script.current++;
            current_script.action_active = 0;
        }
        break;
    case ACTION_PLAY_SOUND_L:
        if (act->time_ms > 0)
        {
            // Esperar hasta que pase el tiempo indicado
            if (current_script.action_timer_ms >= act->time_ms)
            {
                current_script.current++;
                current_script.action_active = 0;
            }
        }
        else
        {
            // Si no tiene tiempo definido, avanzar enseguida
            current_script.current++;
            current_script.action_active = 0;
        }
        break;
    case ACTION_PLAY_MUSIC:
        if (act->time_ms > 0)
        {
            // Esperar hasta que pase el tiempo indicado
            if (current_script.action_timer_ms >= act->time_ms)
            {
                current_script.current++;
                current_script.action_active = 0;
            }
        }
        else
        {
            // Si no tiene tiempo definido, avanzar enseguida
            current_script.current++;
            current_script.action_active = 0;
        }
        break;
    case ACTION_STOP_MUSIC:
        current_script.current++;
        current_script.action_active = 0;
        break;
    case ACTION_SPACE:
        if (act->time_ms > 0)
        {
            // Esperar hasta que pase el tiempo indicado
            if (current_script.action_timer_ms >= act->time_ms)
            {
                current_script.current++;
                current_script.action_active = 0;
            }
        }
        else
        {
            // Si no tiene tiempo definido, avanzar enseguida
            current_script.current++;
            current_script.action_active = 0;
        }
        break;

    default:
        break;
    }
}

void script_reset(void)
{
    end_scene = 0;
    current_script.current = 0;
    current_script.elapsed_ms = 0;
    current_script.action_timer_ms = 0;
}