#ifndef AUDIO_H
#define AUDIO_H

#include <kos.h>
#include <wav/sndwav.h>

void audio_init();
void audio_play_music(const char *filename, int loop);
void audio_stop_music();
void audio_play_sound(const char *filename);
void audio_shutdown();
void audio_play_large_sound(const char *filename, int loop);

extern uint8_t music_volume;
extern uint8_t sound_volume;

#endif
