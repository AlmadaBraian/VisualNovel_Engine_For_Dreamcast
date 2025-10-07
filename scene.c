// scene.c
#include <stdbool.h>
#include "scene.h"
#include "font.h"
#include "cJSON.h"
#include "sprite.h"
#include "script.h"
#include <string.h>
#include <stdio.h>

// Scene scenes[MAX_SCENES];
int current_scene = 0;
int scene_count = 0;
SceneTransition g_transition = {0};
FadeState g_fade;
bool draw_text_box = false;

uint32 w_fuente = 512;
uint32 h_fuente = 372;
pvr_ptr_t tex_font;

// --- Escenario ---
Block textBox = {0.0f, 300.0f, 640.0f, 200.0f};

static inline int min(int a, int b) { return (a < b) ? a : b; }

void change_scene(const char *filename)
{
    // 1. Limpiar escena actual
    scene_clear(&scenes[current_scene]);

    // 2. Cargar nueva escena (fondo + sprites + líneas)
    load_scene_with_textures(&scenes[current_scene], filename);

    // 3. Cargar nuevo script
    load_script(filename);

    // 4. Reiniciar script
    script_reset();
}

void start_scene_transition(void (*callback)(void), int duration_ms)
{
    g_transition.active = 1;
    g_transition.fading_out = 1;
    g_transition.fading_in = 0;
    g_transition.alpha = 0.0f;
    g_transition.alpha_speed = 1.0f / (float)duration_ms;
    g_transition.on_complete = callback;
}

void update_transition(int delta_ms)
{
    if (!g_transition.active)
        return;

    if (g_transition.fading_out)
    {
        g_transition.alpha += g_transition.alpha_speed * delta_ms;
        if (g_transition.alpha >= 1.0f)
        {
            g_transition.alpha = 1.0f;
            g_transition.fading_out = 0;
            g_transition.fading_in = 1;
            if (g_transition.on_complete)
                g_transition.on_complete(); // cambio de escena acá
        }
    }
    else if (g_transition.fading_in)
    {
        g_transition.alpha -= g_transition.alpha_speed * delta_ms;
        if (g_transition.alpha <= 0.0f)
        {
            g_transition.alpha = 0.0f;
            g_transition.active = 0;
            g_transition.fading_in = 0;
        }
    }
}

void draw_transition()
{
    if (!g_transition.active)
        return;
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    uint32 color = ((uint32)(g_transition.alpha * 255) << 24); // ARGB
    // cuadrado pantalla completa
    vert.flags = PVR_CMD_VERTEX;
    vert.x = 0;
    vert.y = 0;
    vert.z = 10;
    vert.argb = color;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.x = 640;
    vert.y = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.x = 0;
    vert.y = 480;
    pvr_prim(&vert, sizeof(vert));

    vert.x = 640;
    vert.y = 480;
    vert.flags = PVR_CMD_VERTEX_EOL;
    pvr_prim(&vert, sizeof(vert));
}

void scene_clear(Scene *scene)
{
    // Liberar fondo actual si existe
    if (scene->bg_tex)
    {
        pvr_mem_free(scene->bg_tex);
        scene->bg_tex = NULL;
    }

    // Liberar texturas de sprites
    for (int i = 0; i < scene->sprite_count; i++)
    {
        if (scene->sprites[i].tex)
        {
            pvr_mem_free(scene->sprites[i].tex);
            scene->sprites[i].tex = NULL;
        }
    }

    memset(scene->sprites, 0, sizeof(scene->sprites));
    scene->sprite_count = 0;

    // Resetear texto
    if (scene->text_lines)
    {
        for (int i = 0; i < scene->text_line_count; i++)
        {
            free(scene->text_lines[i]);
        }
        free(scene->text_lines);
        scene->text_lines = NULL;
    }
    scene->line_count = 0;
    scene->current_line = 0;
    scene->chars_displayed = 0;
    scene->text_lines = NULL;
    scene->text_line_index = 0;
    scene->text_line_count = 0;
}

void scene_init(Scene *scene)
{
    scene->bg_tex = NULL;
    scene->bg_w = scene->bg_h = 0;
    scene->sprite_count = 0;
    scene->line_count = 0;
    scene->current_line = 0;
    scene->chars_displayed = 0;
}

void scene_add_sprite(const char *name, Scene *scene, pvr_ptr_t tex, float x, float y, float w, float h,
                      uint32 tex_w, uint32 tex_h, float alpha, float alpha_speed,
                      int fading_out, int fading_in, const char *file, int fondo)
{

    if (scene->sprite_count >= 16)
    {
        printf("⚠ No se pueden agregar más sprites, límite alcanzado.\n");
        return;
    }

    Sprite *s = &scene->sprites[scene->sprite_count++];
    memset(s, 0, sizeof(Sprite)); // Limpia memoria por seguridad

    // --- Nombre del sprite ---
    if (name)
    {
        strncpy(s->name, name, sizeof(s->name) - 1);
        s->name[sizeof(s->name) - 1] = '\0';
    }

    s->visible = 1;
    s->alpha = alpha;
    s->alpha_speed = alpha_speed;
    s->fading_out = fading_out;
    s->fading_in = fading_in;
    s->x = x;
    s->y = y;
    s->width = w;
    s->height = h;
    s->tex_w = tex_w;
    s->tex_h = tex_h;
    s->fondo = fondo;

    // --- Asignar textura ---
    s->tex = tex;
    if (!s->tex)
    {
        printf("⚠ Error: no se pudo cargar la textura para sprite '%s'%s%s\n",
               name ? name : "(sin nombre)",
               file ? " (" : "",
               file ? file : "");
    }

    // --- Guardar ruta de archivo ---
    if (file)
    {
        strncpy(s->file, file, sizeof(s->file) - 1);
        s->file[sizeof(s->file) - 1] = '\0';
    }
    else
    {
        s->file[0] = '\0'; // Dejar en vacío si no se pasa file
    }
}

void scene_next_line(Scene *scene)
{
    if (scene->current_line < scene->line_count - 1)
    {
        scene->current_line++;
        scene->chars_displayed = 0; // reinicia efecto máquina de escribir
    }
}

// -------------------
// CARGA DESDE JSON
// -------------------
void load_scene_from_json(Scene *scene, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        printf("No pude abrir %s\n", filename);
        return;
    }

    // load_script(filename);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *json_data = malloc(size + 1);
    if (!json_data)
    {
        fclose(f);
        return;
    }

    fread(json_data, 1, size, f);
    json_data[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(json_data);
    if (!root)
    {
        printf("Error parseando JSON: %s\n", cJSON_GetErrorPtr());
        free(json_data);
        return;
    }

    // --- Fondo ---
    cJSON *bg = cJSON_GetObjectItem(root, "background");
    if (!cJSON_IsString(bg) || !bg->valuestring || strlen(bg->valuestring) == 0)
    {
        printf("⚠ Sprite sin textura válida, se salta.\n");
    }
    else
    {
        if (bg->valuestring[0] != '\0')
        {
            // ruta válida, cargar textura
            uint32 tex_w, tex_h;
            printf("Intentando abrir: %s\n", bg->valuestring);
            scene->bg_tex = load_png_texture(bg->valuestring, &scene->bg_w, &scene->bg_h, &tex_w, &tex_h);
        }
        else
        {
            // ruta vacía: dibujar bloque negro
            scene->bg_tex = 0; // no hay textura
            scene->bg_w = 640; // ancho de pantalla
            scene->bg_h = 480; // alto de pantalla
        }
    }

    // --- Sprites ---
    cJSON *sprites = cJSON_GetObjectItem(root, "sprites");
    if (cJSON_IsArray(sprites))
    {
        int count = cJSON_GetArraySize(sprites);
        scene->sprite_count = 0;

        for (int i = 0; i < count; i++)
        {
            cJSON *spr = cJSON_GetArrayItem(sprites, i);
            if (!cJSON_IsObject(spr))
                continue;

            // --- Lectura de datos obligatorios ---
            const char *tex_path = cJSON_GetObjectItem(spr, "texture")->valuestring;
            const char *name = cJSON_GetObjectItem(spr, "name")->valuestring;

            // --- Fallback de file ---
            cJSON *file_item = cJSON_GetObjectItem(spr, "file");
            const char *file = (file_item && cJSON_IsString(file_item))
                                   ? file_item->valuestring
                                   : tex_path; // usar la textura como "file" si no hay campo

            printf("Intentando abrir Sprite: %s\n", tex_path);
            Sprite *sprite = malloc(sizeof(Sprite));
            memset(sprite, 0, sizeof(Sprite));
            // --- Carga de textura ---
            uint32 tex_w = 0, tex_h = 0;
            // --- Lectura de transformaciones ---
            float x = cJSON_GetObjectItem(spr, "x")->valuedouble;
            float y = cJSON_GetObjectItem(spr, "y")->valuedouble;
            float w = cJSON_GetObjectItem(spr, "width")->valuedouble;
            float h = cJSON_GetObjectItem(spr, "height")->valuedouble;

            pvr_ptr_t tex = load_png_texture(tex_path, &tex_w, &tex_h, &tex_w, &tex_h);
            if (!tex)
            {
                printf("Error cargando sprite %s (no se pudo abrir %s)\n", name, tex_path);
                continue; // saltar sprite si no carga
            }

            // --- Propiedades opcionales ---
            cJSON *alpha_item = cJSON_GetObjectItem(spr, "alpha");
            float alpha = (alpha_item && cJSON_IsNumber(alpha_item)) ? alpha_item->valuedouble : 1.0f;

            cJSON *alpha_speed_item = cJSON_GetObjectItem(spr, "alpha_speed");
            float alpha_speed = (alpha_speed_item && cJSON_IsNumber(alpha_speed_item)) ? alpha_speed_item->valuedouble : 0.0f;

            cJSON *fading_out_item = cJSON_GetObjectItem(spr, "fading_out");
            int fading_out = (fading_out_item && cJSON_IsBool(fading_out_item)) ? fading_out_item->valueint : 0;

            cJSON *fading_in_item = cJSON_GetObjectItem(spr, "fading_in");
            int fading_in = (fading_in_item && cJSON_IsBool(fading_in_item)) ? fading_in_item->valueint : 0;

            cJSON *fondo_item = cJSON_GetObjectItem(spr, "fondo");
            int fondo = (fondo_item && cJSON_IsNumber(fondo_item)) ? fondo_item->valueint : 0;

            // --- Agregar sprite a la escena ---
            scene_add_sprite(name, scene, tex, x, y, w, h, tex_w, tex_h,
                             alpha, alpha_speed, fading_out, fading_in, file, fondo);
        }
    }

    // --- Líneas ---
    cJSON *lines = cJSON_GetObjectItem(root, "lines");
    if (cJSON_IsArray(lines))
    {
        scene->line_count = 0;
        char *buf_ptr = scene->line_buffer; // puntero dentro del buffer grande
        int count = cJSON_GetArraySize(lines);
        for (int i = 0; i < count; i++)
        {
            cJSON *ln = cJSON_GetArrayItem(lines, i);
            if (cJSON_IsString(ln))
            {
                const char *txt = ln->valuestring;

                // --- Saltar BOM si existe ---
                if ((unsigned char)txt[0] == 0xEF &&
                    (unsigned char)txt[1] == 0xBB &&
                    (unsigned char)txt[2] == 0xBF)
                {
                    txt += 3;
                }

                // Copiar línea al buffer grande
                size_t len = strlen(txt);
                if (len >= MAX_LINE_LEN)
                    len = MAX_LINE_LEN - 1;
                memcpy(buf_ptr, txt, len);
                buf_ptr[len] = '\0';

                // Guardar puntero en arreglo de líneas
                scene->lines[scene->line_count++] = buf_ptr;

                // Avanzar puntero
                buf_ptr += MAX_LINE_LEN;
            }
        }
    }

    cJSON_Delete(root);
    free(json_data);
}

void load_scene_with_textures(Scene *scene, const char *json_file)
{
    // Primero liberar texturas viejas
    for (int i = 0; i < scene->sprite_count; i++)
        sprite_free_texture(&scene->sprites[i]);

    load_scene_from_json(scene, json_file);
}

// -------------------
// RENDER
// -------------------
void scene_render(Scene *scene)
{
    // --- Fondo ---
    pvr_list_begin(PVR_LIST_OP_POLY);
    if (scene->bg_tex)
    {
        draw_sprite(0, 0, scene->bg_w, scene->bg_h,
                    scene->bg_w, scene->bg_h,
                    next_pow2(scene->bg_w), next_pow2(scene->bg_h),
                    scene->bg_tex, PVR_LIST_OP_POLY, 1.0f);
    }

    pvr_list_finish();

    // --- Sprites ---
    pvr_list_begin(PVR_LIST_TR_POLY);
    // dibujar todos los sprites
    for (int i = 0; i < scene->sprite_count; i++)
    {
        Sprite *s = &scene->sprites[i];
        if (s->visible)
        {
            if (s->fondo == 0)
            {
                draw_sprite(s->x, s->y, s->width, s->height,
                            s->tex_w, s->tex_h,
                            next_pow2(s->tex_w), next_pow2(s->tex_h),
                            s->tex, PVR_LIST_TR_POLY, s->alpha);
            }
            else
            {
                draw_sprite(s->x, s->y, 640.0f, 480.0f,
                            s->width, s->height,
                            next_pow2(s->tex_w), next_pow2(s->tex_h),
                            s->tex, PVR_LIST_TR_POLY, s->alpha);
            }
        }
    }

    // --- Caja de texto y línea actual ---
    if (draw_text_box)
    {
        draw_block(&textBox, 0x80000000);
    }

    if (scene->text_lines && scene->text_line_index < scene->text_line_count)
    {
        const char *line = scene->text_lines[scene->text_line_index];
        int len = (scene->chars_displayed < strlen(line)) ? scene->chars_displayed : strlen(line);
        char buf[256];
        memcpy(buf, line, len);
        buf[len] = '\0';
        // debug_draw_script(20.0f, 30.0f);
        draw_string(20, 340, buf, len, &len, 1.0f);
    }
    if (showButton)
    {
        draw_sprite_anim(
            570, 420, // posición
            42, 42,   // tamaño en pantalla (puede ser diferente)
            7, 8,     // columna/frame = 1, 8 columnas totales
            bt_tex_w, bt_tex_h,
            buttons_tex,
            0, 4 // fila = 1, total de filas = 4
        );
    }

    draw_transition();

    pvr_list_finish();
}

void draw_fade_overlay(float alpha)
{
    if (alpha <= 0.0f)
        return;

    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    // Contexto para polígonos sin textura, en la lista transparente
    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    // Calcula ARGB: negro con alpha escalado 0-255
    uint32 a = (uint32)(alpha * 255.0f);
    uint32 color = (a << 24) | 0x000000; // 0xAA000000

    // Vértice superior izquierdo
    vert.flags = PVR_CMD_VERTEX;
    vert.x = 0.0f;
    vert.y = 0.0f;
    vert.z = 10.0f;
    vert.u = vert.v = 0.0f;
    vert.argb = color;
    vert.oargb = 0;
    pvr_prim(&vert, sizeof(vert));

    // Vértice superior derecho
    vert.x = 640.0f;
    vert.y = 0.0f;
    pvr_prim(&vert, sizeof(vert));

    // Vértice inferior izquierdo
    vert.x = 0.0f;
    vert.y = 480.0f;
    pvr_prim(&vert, sizeof(vert));

    // Vértice inferior derecho (último, con EOL)
    vert.x = 640.0f;
    vert.y = 480.0f;
    vert.flags = PVR_CMD_VERTEX_EOL;
    pvr_prim(&vert, sizeof(vert));
}

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
