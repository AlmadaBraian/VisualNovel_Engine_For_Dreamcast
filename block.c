// block.c
#include "block.h"

void draw_block(Block *b, uint32 color) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    // Configurar polígono
    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&hdr, &cxt);

    pvr_prim(&hdr, sizeof(hdr));

    // Esquinas
    pvr_vertex_t v = {
        .x = b->x,
        .y = b->y,
        .z = 0,
        .argb = color,
        .oargb = color
    };
    pvr_prim(&v, sizeof(v));

    v.x = b->x + b->w;
    v.y = b->y;
    pvr_prim(&v, sizeof(v));

    v.x = b->x + b->w;
    v.y = b->y + b->h;
    pvr_prim(&v, sizeof(v));

    v.x = b->x;
    v.y = b->y + b->h;
    pvr_prim(&v, sizeof(v));
}
