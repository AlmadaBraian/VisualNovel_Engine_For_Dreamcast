#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <kos.h>

#define MAX_PATH_LEN 256

typedef struct {
    pvr_ptr_t tex;
    uint32 w, h;
} FrameSlot;

typedef struct {
    int frame_count;
    int current_frame;
    int fps;
    int frame_duration_ms;
    int elapsed_ms;
    int finished;

    char path[MAX_PATH_LEN];
    FrameSlot current;
} VideoPlayer;

void video_init(VideoPlayer *vp, const char *path, int frame_count, int fps, const char *audio_file);
void video_update(VideoPlayer *vp, int delta_ms);
void video_draw(VideoPlayer *vp);
void video_shutdown(VideoPlayer *vp);

#endif
