#ifndef SPRITE_H
#define SPRITE_H

#include <kos.h>

// Tipo para un sprite
typedef struct {
    char name[32];
    float x, y;
    uint32 width, height;
    uint32 tex_w, tex_h;
    pvr_ptr_t tex;
    int visible;
 // Para animación
    float target_x, target_y;
	float speed_x, speed_y;   // velocidad por frame
    int animating;            // flag de animación
	float alpha;          // valor actual de opacidad (0.0 a 1.0)
	float alpha_speed;    // cuánto decrece por frame
	int fading_out;       // flag si está desvaneciéndose
	int fading_in;       // flag si está desvaneciéndose
    char file[128];   // archivo PNG asociado
    int fondo;
    float scale;
} Sprite;

typedef struct {
    float x, y;
    float width, height;
} Block;

static inline uint32 next_pow2(uint32 x) {
    uint32 r = 1;
    while(r < x) r <<= 1;
    return r;
}

extern Block fondoMenu;

void draw_block(Block *b, uint32 color);

// Prototipos
pvr_ptr_t load_png_texture(const char *filename, uint32 *w, uint32 *h, uint32 *tex_w, uint32 *tex_h);
void draw_sprite(float x, float y, float w, float h,
                 uint32 src_w, uint32 src_h,
                 uint32 tex_w, uint32 tex_h,
                 pvr_ptr_t tex, int list, float alpha, float capa);
pvr_ptr_t create_black_texture(uint32 *w, uint32 *h);

void sprite_load_texture(Sprite *s, const char *file);

void sprite_free_texture(Sprite *s);

void draw_sprite_anim(float x, float y, float draw_w, float draw_h,
                      int frame, int frames_per_row, uint32 tex_w, uint32 tex_h,
                      pvr_ptr_t tex, int row, int total_rows, float capa);


#endif
