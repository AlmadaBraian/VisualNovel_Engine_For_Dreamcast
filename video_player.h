#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <kos.h>
#include "menu.h"
#include "mpegDC.h" // sólo la cabecera, la implementación debe estar en un único .c

#define MAX_PATH_LEN 256

#define VIDEO_W 256
#define VIDEO_H 128
#define SCREEN_W 640
#define SCREEN_H 380
#define UV_EPSILON 0.001f

/* Variables que están DEFINIDAS en main.c (declaralas como extern aquí) */
extern int video_width;
extern int video_height;
extern int state;
extern Menu main_menu;
extern int menu_active;
extern int playing_video;
extern int video_destroyed;
extern plm_t *mpeg;
extern pvr_vertex_t vert[4];
extern pvr_ptr_t video_tex;

/* Variables PVR */
extern pvr_poly_cxt_t cxt;
extern pvr_poly_hdr_t hdr;

void play_video(const char *filenameVideo, const char *filenameAudio);
void on_video_fadeout_complete(void);
void yuv420_to_yuv422(plm_frame_t *frame, uint16_t *vram);
void render_video_frame(plm_frame_t *video_frame);

#endif /* VIDEO_PLAYER_H */
