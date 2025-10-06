// video_player.h
#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <kos.h>

#define BUFFER_SIZE   20
#define MAX_PATH_LEN  256
#define MAX_FRAMES    10000   // número máximo de frames que vas a manejar

// --- info de cada frame en el .bin ---
typedef struct {
    uint32 time_ms;   // tiempo del frame en milisegundos
    uint16 index;     // índice a la lista de nombres
} FrameInfo;

// --- slot en buffer de texturas ---
typedef struct {
    pvr_ptr_t tex;      // textura en VRAM
    int frame_index;    // índice del frame que contiene
    uint32 w, h;        // dimensiones
    int valid;          // si está en uso
} FrameSlot;

// --- reproductor de video ---
typedef struct {
    int fps;
    int frame_count;
    int current_index;
    int finished;

    int frame_duration_ms;
    int elapsed_ms;

    char path[MAX_PATH_LEN];

    // buffer circular
    FrameSlot buffer[BUFFER_SIZE];
    int buffer_start;
    int buffer_end;
    int buffer_count; 
    // datos cargados del .bin y .names
    FrameInfo *frames;     // array de frames con tiempos/índices
    uint32 *frame_times;   // tiempos absolutos acumulados
    uint16 *frame_indices; // índices a frame_names

    char **frame_names;    // lista de nombres de archivo
    int frame_name_count;  // cantidad de nombres leídos

    int name_count;        // para liberar en shutdown
} VideoPlayer;

// --- funciones públicas ---
void video_init(VideoPlayer *vp, const char *path, int fps,
                const char *bin_file, const char *names_file,
                const char *audio_file);

int video_load_frames(VideoPlayer *vp, const char *bin_file, const char *names_file);

void video_update(VideoPlayer *vp, int delta_ms);

void video_draw(VideoPlayer *vp);

void video_shutdown(VideoPlayer *vp);

#endif
