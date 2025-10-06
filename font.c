#include <kos.h>
#include <stdlib.h>
#include "wfont_widths.h"   // Ancho real de cada glifo
#include "wfont_offsets.h"
#include "wfont_uvs.h"// Offset x/y de cada glifo
#include "wfont_metrics.h"
#include "sprite.h"

pvr_ptr_t font_tex;
uint32 w_font_image;
uint32 h_font_image;

#define WFONT_CHARS_PER_ROW 12
#define WFONT_CELL_HEIGHT 39
#define FONT_IMG_PATH "/rd/wfont_visual.png"

// --- UTF-8 simple (ASCII + 2 bytes) ---
int utf8_next(const char *s, unsigned char *out) {
    unsigned char c = (unsigned char)s[0];
    if(c < 0x80) { *out = c; return 1; }
    if((c & 0xE0) == 0xC0) {
        unsigned char c2 = (unsigned char)s[1];
        *out = ((c & 0x1F) << 6) | (c2 & 0x3F);
        return 2;
    }
    *out = '?';
    return 1;
}

// --- Convertir UTF-8 a índice en atlas ---
unsigned char utf8_to_font_index(const char **p) {
    unsigned char c = (unsigned char)**p;

    if(c >= 32 && c <= 127) { (*p)++; return c - 32; }

    if((c & 0xE0) == 0xC0) {
        unsigned char c2 = (unsigned char)*(*p + 1);
        (*p) += 2;
        uint16_t code = (c << 8) | c2;
        switch(code) {
            case 0xC3B1: return 96;  // ñ
            case 0xC3A1: return 97;  // á
            case 0xC3A9: return 98;  // é
            case 0xC3AD: return 99;  // í
            case 0xC3B3: return 100; // ó
            case 0xC3BA: return 101; // ú
            case 0xC391: return 102; // Ñ
            case 0xC381: return 103; // Á
            case 0xC389: return 104; // É
            case 0xC38D: return 105; // Í
            case 0xC393: return 106; // Ó
            case 0xC39A: return 107; // Ú
			case 0xC2A1: return 108; // ¡
			case 0xC2BF: return 109; // ¿
            default: return 0;        // '?'
        }
    }

    (*p)++;
    return 0; // '?'
}

// --- Inicializar fuente ---
void font_init(void) {
    w_font_image = 0;
    h_font_image = 0;
    font_tex = load_png_texture(FONT_IMG_PATH, &w_font_image, &h_font_image,
                                &w_font_image, &h_font_image);
}



// --- Dibujar un carácter alineado a la línea base ---
void draw_char(float x, float y, int c, float scale) {
       // ancho real del glifo en pantalla = advance * scale
    float char_w = wfont_widths[c] * scale;
    // altura de celda (fija)
    float char_h = (float)WFONT_CELL_HEIGHT * scale;

    // gx = posición horizontal del pen (no aplicamos offset_x aquí porque
    // al generar el atlas usamos offset_x = -bbox[0] dejando el glifo
    // alineado al borde izquierdo de la celda)
    float gx = x;
     // gy: para alinear la baseline hay que desplazar por:
    // top_of_quad = baseline_target - baseline_in_cell + glyph_top_in_cell
    // y (parámetros):
    //   y = baseline_target (porque draw_string pasa y como baseline)
    //   WFONT_BASELINE = baseline en píxeles desde TOP de la celda (generado por Python)
    //   glyph_offsets[c].y = offset_y usado al dibujar en atlas (en píxeles desde top)
    float gy = y - (WFONT_BASELINE * scale) + (glyph_offsets[c].y * scale);

    float u0 = glyph_uvs[c].u0;
    float v0 = glyph_uvs[c].v0;
    float u1 = glyph_uvs[c].u1;
    float v1 = glyph_uvs[c].v1;

    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED,
                     w_font_image, h_font_image, font_tex, PVR_FILTER_NONE);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    vert.argb = 0xFFFFFFFF; vert.oargb = 0; vert.flags = PVR_CMD_VERTEX;

    vert.x = gx;        vert.y = gy;        vert.z = 1.0f; vert.u = u0; vert.v = v0; pvr_prim(&vert,sizeof(vert));
    vert.x = gx+char_w; vert.y = gy;        vert.u = u1; vert.v = v0; pvr_prim(&vert,sizeof(vert));
    vert.x = gx;        vert.y = gy+char_h; vert.u = u0; vert.v = v1; pvr_prim(&vert,sizeof(vert));
    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = gx+char_w; vert.y = gy+char_h; vert.u = u1; vert.v = v1; pvr_prim(&vert,sizeof(vert));
}

// --- Dibujar string UTF-8 alineado a la línea base ---
void draw_string(float x, float y, const char *str, int max_chars, int *chars_displayed, float text_scale) {
    float start_x = x;
    int count = 0;

    while (*str && count < max_chars) {
        // manejar '\n' directamente
        if (*str == '\n') {
            str++;
            x = start_x;
            y += WFONT_CELL_HEIGHT * text_scale;
            continue;
        }
		char ch = *str;
        unsigned char idx = utf8_to_font_index(&str);

        if (idx >= 0) {
			if (ch == ' ') {
			// Avance manual para el espacio
			x += (WFONT_CELL_WIDTH / 3.0f) * text_scale;  // o usa wfont_widths[idx] si tenés el avance correcto en el JSON
			} else {
				draw_char(x, y, idx, text_scale);
				x += wfont_widths[idx] * text_scale;
			}
		}
        count++;
    }

    if (chars_displayed) *chars_displayed = count;
}

