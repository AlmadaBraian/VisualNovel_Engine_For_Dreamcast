// wfont.h
#ifndef WFONT_H
#define WFONT_H

#include <kos.h>

// --- Configuración del atlas ---
#define WFONT_ATLAS_W        512   // Ancho del atlas en píxeles (tu PNG)
#define WFONT_ATLAS_H        512   // Alto del atlas en píxeles (tu PNG)
#define WFONT_CELL_WIDTH     32    // Ancho de cada celda (en px)
#define WFONT_CELL_HEIGHT    31    // Alto de cada celda (en px)
#define WFONT_NUM_CHARS      256   // Cantidad de caracteres en el atlas
#define WFONT_BASELINE       12    // Línea base sugerida para alinear texto

// --- Macros útiles ---
#define WFONT_COLS           (WFONT_ATLAS_W / WFONT_CELL_WIDTH)
#define WFONT_ROWS           (WFONT_ATLAS_H / WFONT_CELL_HEIGHT)

// Cuántos bytes ocupa la tabla de anchos si la guardás en bits
#define WFONT_WIDTH_BYTES    (WFONT_NUM_CHARS / 8)

// --- Estructura de UVs ---
typedef struct {
    float u0, v0;
    float u1, v1;
} wfont_uv_t;

// Declaración externa de la tabla de UVs generada por el script
extern const wfont_uv_t wfont_uvs[WFONT_NUM_CHARS];

#endif // WFONT_H
