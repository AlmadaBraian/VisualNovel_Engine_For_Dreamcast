#include <png/png.h>
#include "sprite.h"

// --- Configuración del atlas ---
#define FONT_IMG_W 512
#define FONT_IMG_H 372
#define FONT_CHAR_W 16   // ancho fijo en tu atlas, ajustar si tu script usa otro
#define FONT_CHAR_H 16   // alto de cada carácter


// --- Crear textura negra semi-transparente 2x2 ---
pvr_ptr_t create_black_texture(uint32 *w, uint32 *h)
{
    *w = 2; *h = 2;
    pvr_ptr_t tex = pvr_mem_malloc(2 * (*w) * (*h));
    if(!tex) return NULL;
    uint16 *ptr = (uint16*)tex;
    for(int i=0;i<(*w)*(*h);i++) ptr[i] = 0x8000; // negro semi-transparente ARGB4444
    return tex;
}

// --- Cargar PNG a textura PVR ---
pvr_ptr_t load_png_texture(const char *filename, uint32 *w, uint32 *h, uint32 *tex_w, uint32 *tex_h)
{
    kos_img_t img;

    // --- Esto necesita incluir <png/png.h> para PNG_FULL_ALPHA y png_to_img ---
    if(png_to_img(filename, PNG_FULL_ALPHA, &img) < 0) {
        printf("Error cargando %s\n", filename);
        return NULL;
    }

    *w = img.w; *h = img.h;
    *tex_w = next_pow2(img.w);
    *tex_h = next_pow2(img.h);

    pvr_ptr_t tex = pvr_mem_malloc((*tex_w) * (*tex_h) * 2);
    if(!tex) { free(img.data); return NULL; }

    memset((void*)tex, 0, (*tex_w) * (*tex_h) * 2);

    uint16 *dst = (uint16*)tex;
    uint16 *src = (uint16*)img.data;
    for(uint32 y=0;y<img.h;y++)
        for(uint32 x=0;x<img.w;x++)
            dst[y * (*tex_w) + x] = src[y * img.w + x];

    free(img.data);
    return tex;
}

// --- Dibujar sprite rectangular ---
void draw_sprite(float x, float y, float w, float h, uint32 sprite_w, uint32 sprite_h, uint32 tex_w, uint32 tex_h, pvr_ptr_t tex, int list, float alpha)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    pvr_poly_cxt_txr(&cxt, list, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED,
                     tex_w, tex_h, tex, PVR_FILTER_BILINEAR);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    float u1 = (float)sprite_w / tex_w;
    float v1 = (float)sprite_h / tex_h;
	
	// Clamp alpha entre 0 y 1
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    uint32 argb = ((uint32)(alpha * 0xFF) << 24) | 0x00FFFFFF;  // RGB intacto, solo alpha

    vert.argb = argb;
    vert.oargb = 0;
    vert.flags = PVR_CMD_VERTEX;

    vert.x = x; vert.y = y; vert.z = 1.0f; vert.u = 0; vert.v = 0; pvr_prim(&vert, sizeof(vert));
    vert.x = x + w; vert.y = y; vert.u = u1; vert.v = 0; pvr_prim(&vert, sizeof(vert));
    vert.x = x; vert.y = y + h; vert.u = 0; vert.v = v1; pvr_prim(&vert, sizeof(vert));
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = x + w; vert.y = y + h; vert.u = u1; vert.v = v1; pvr_prim(&vert, sizeof(vert));
}

// --- Dibujar bloque sólido ---
void draw_block(Block *b, uint32 color)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    vert.flags = PVR_CMD_VERTEX;
    vert.argb = color; vert.oargb = 0;

    vert.x = b->x; vert.y = b->y; vert.z = 1.0f; pvr_prim(&vert,sizeof(vert));
    vert.x = b->x + b->width; vert.y = b->y; pvr_prim(&vert,sizeof(vert));
    vert.x = b->x; vert.y = b->y + b->height; pvr_prim(&vert,sizeof(vert));
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = b->x + b->width; vert.y = b->y + b->height; pvr_prim(&vert,sizeof(vert));
}

void sprite_load_texture(Sprite *s, const char *file) {
    s->tex = load_png_texture(file, &s->tex_w, &s->tex_h, &s->tex_w, &s->tex_h);
    if(s->tex == 0) {
        printf("Error cargando sprite %s\n", file);
    } else {
        strncpy(s->file, file, sizeof(s->file)-1);
        s->file[sizeof(s->file)-1] = '\0';
    }
}

// Liberar textura
void sprite_free_texture(Sprite *s) {
    if(s->tex) {
        pvr_mem_free(s->tex);
        s->tex = 0;
    }
}





