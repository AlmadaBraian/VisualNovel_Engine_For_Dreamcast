#ifndef SCENE_H
#define SCENE_H

#include <kos.h>
#include <dc/pvr.h>
#include <string.h>

#include "sprite.h" // si Sprite está en otro header
#include "cJSON.h"

#define MAX_SCENES 16
#define MAX_LINES 128
#define MAX_CHARS 100
#define MAX_LINE_LEN 256

extern int current_scene;

extern pvr_ptr_t buttons_tex;
extern uint32 bt_tex_w, bt_tex_h;
extern bool draw_text_box;
extern bool draw_loading_screen;

typedef struct
{
    pvr_ptr_t bg_tex;
    uint32 bg_w, bg_h;
    Sprite sprites[16];
    int sprite_count;
    char character_name[32];
    const char *lines[MAX_LINES];
    char line_buffer[MAX_LINES * MAX_LINE_LEN]; // buffer grande para copiar todas las líneas
    int line_count;

    // Para efecto de máquina de escribir
    int current_line;
    int chars_displayed;
    // --- NUEVO: para show_text con múltiples líneas ---
    char **text_lines;     // puntero a las líneas de la acción actual
    int text_line_count;   // cuántas líneas tiene esta acción
    int text_line_index;   // índice de la línea dentro de la acción
    int text_action_index; // índice de la acción show_text actual
    bool waiting_for_input;
    int line_complete;
    int just_started_text;
} Scene;

typedef struct
{
    int active;
    int fading_out;            // 1 si estamos oscureciendo
    int fading_in;             // 1 si estamos aclarando
    float alpha;               // 0.0 = transparente, 1.0 = pantalla negra
    float alpha_speed;         // velocidad de cambio por ms
    void (*on_complete)(void); // callback opcional para cuando termina
} SceneTransition;

extern SceneTransition g_transition;

void start_scene_transition(void (*callback)(void), int duration_ms);

void update_transition(int delta_ms);

void draw_transition();

void load_scene_from_json(Scene *scene, const char *filename);

void free_scene(Scene *scene);

void change_scene(const char *filename);

void load_scene_with_textures(Scene *scene, const char *json_file);

extern Scene scenes[MAX_SCENES];
extern int current_scene;
extern int scene_count;

// Funciones de manejo de escenas
void scene_init(Scene *scene);
void scene_add_sprite(const char *name, Scene *scene, pvr_ptr_t tex, float x, float y, float w, float h,
                      uint32 tex_w, uint32 tex_h, float alpha, float alpha_speed, int fading_out, int fading_in, const char *file, int fondo, float scale);
void scene_add_line(Scene *scene, const char *line);
void scene_next_line(Scene *scene);
void scene_render(Scene *scene);

void draw_fade_overlay(float alpha);

typedef struct
{
    int active;
    float alpha;
    int duration_ms;
    int elapsed_ms;
    int fading_out; // 1 si estamos oscureciendo
    int fading_in;
    void (*on_complete)(void);
} FadeState;

extern FadeState g_fade;

void free_scene_resources(Scene *scene);

void fade_update(int delta_ms);

#endif // SCENE_H
