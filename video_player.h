// video_player.h
#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <kos.h>
#include <kos/thread.h>
#include <kos/sem.h>

#define MAX_PATH_LEN 256

typedef struct {
    kthread_t *loader_thread;
    semaphore_t load_semaphore;
    semaphore_t load_complete;

    pvr_ptr_t next_tex;
    uint32 next_w, next_h;
    uint32 next_tex_w, next_tex_h;
    char file_to_load[256];
    int frame_to_load;
    int loading;
    kos_img_t staged_img;
    volatile int staged_ready;
} VideoLoader;

typedef struct {
    int fps;
    int frame_count;
    int current_frame;
    int finished;
    int frame_duration_ms;
    uint32 start_time_ms;

    char path[MAX_PATH_LEN];

    pvr_ptr_t tex;
    uint32 w, h;
    uint32 tex_w, tex_h;

    VideoLoader loader;
} VideoPlayer;

void video_init(VideoPlayer *vp, const char *path, int frame_count, int fps, const char *audio_file);
void video_update(VideoPlayer *vp);
void video_draw(VideoPlayer *vp);
void video_shutdown(VideoPlayer *vp);

pvr_ptr_t video_load_texture(const char *filename, uint32 *w, uint32 *h, uint32 *tex_w, uint32 *tex_h);
void video_draw_sprite(float x, float y, float w, float h, uint32 tex_w, uint32 tex_h, pvr_ptr_t tex, int list, float alpha);

void video_request_load(VideoPlayer *vp, int frame_num);
pvr_ptr_t video_load_kmg(const char *filename, uint32 *w, uint32 *h, uint32 *tex_w, uint32 *tex_h);

#endif
