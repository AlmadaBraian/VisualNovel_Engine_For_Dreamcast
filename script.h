#ifndef SCRIPT_H
#define SCRIPT_H

#include <kos.h>
#include <stdbool.h>
#include "scene.h"
#include "audio.h"
#define MAX_TEXT_LINES 16  // máximo de líneas por acción


typedef enum {
    ACTION_SHOW_SPRITE,
	ACTION_OFF_SPRITE,
    ACTION_PLAY_SOUND,
    ACTION_ANIMATE,
    ACTION_SHOW_TEXT,
    ACTION_PLAY_MUSIC,
	ACTION_STOP_MUSIC,
	ACTION_NEXT_SCENE
} ActionType;

typedef struct {
    int time_ms;
    ActionType type;
    char sprite[64];
    char anim[64];
    char sound[64];
    char music[64];
    int x, y;
    int loop;
        // Para texto tipo array
    int text_line_count;
    char *text_lines[MAX_TEXT_LINES];
	bool wait_input;
	char scene[64];
	char scene_new[128];
} ScriptAction;

typedef struct {
    ScriptAction *actions;
    int count;
    int current;
    int elapsed_ms;
	int action_active;
	int action_timer_ms;
} Script;

extern char next_scene_name[128]; // buffer temporal para el nombre

extern Script current_script;

const char *script_get_current_text();

void change_scene_callback(void);

void scene_clear(Scene *scene);

ActionType parse_action_type(const char *str);

void load_script(const char *filename);

// animation.h
void start_animation(Scene *scene, const char *sprite_name, const char *anim_name, int final_x, int final_y, int delta_ms);

// audio.h
void play_sound(const char *filename);
void play_music(const char *filename, int loop);

void script_update(Scene *scene, int delta_ms, int button_pressed);

void script_reset(void);

void update_animations(Scene *scene, int delta_ms);

#endif // SCRIPT_H