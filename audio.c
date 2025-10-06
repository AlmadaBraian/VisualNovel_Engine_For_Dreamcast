#include "audio.h"
#include <stdio.h>
#include <string.h>
#include <kos/thread.h>  // para thd_sleep()
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>

#define CENTER 255

#define LOOP 1

static wav_stream_hnd_t musica = -1;

sfxhnd_t sound_file_move;
sfxhnd_t sound_file_acept;
//uint8_t volume = 128;
uint8_t music_volume = 128;
uint8_t sound_volume = 128;

void audio_init() {
    wav_init();
	snd_init();
	sound_file_move = snd_sfx_load("/rd/fx_98_pcm.wav");
	sound_file_acept = snd_sfx_load("/rd/fx_06_pcm.wav");
}


void audio_play_music(const char *filename, int loop) {
    if (!filename) return;

    // Si ya había música sonando, detenerla primero
    if (musica >= 0) {
        printf("Deteniendo música previa...\n");
        wav_stop(musica);
        wav_destroy(musica);
        thd_sleep(1); // esperar un frame para que libere bien
        musica = -1;
    }

    // Crear nuevo stream
    musica = wav_create(filename, loop ? LOOP : 0);
    if (musica >= 0) {
        printf("Reproduciendo música: %s\n", filename);
		wav_volume(musica, music_volume);
        wav_play(musica);
    } else {
        printf("Error: no se pudo crear handle para %s\n", filename);
    }
}

void audio_stop_music() {
    if (musica >= 0) {
        printf("Deteniendo música manualmente...\n");

        // Pedir al mixer que pare
        wav_stop(musica);

        // Esperar un par de frames para que el hilo de audio termine
        for (int i = 0; i < 3; i++) {
            thd_sleep(1);  // 1 frame (~16 ms)
        }

        // Destruir el handle una vez que el stream está parado
        wav_destroy(musica);
        musica = -1;

    } else {
        printf("No había música sonando.\n");
    }
}


void audio_play_sound(const char *filename) {
	if (strcmp(filename, "move_cursor") == 0) {
        snd_sfx_play(sound_file_move, sound_volume, CENTER);
    } else if (strcmp(filename, "acept") == 0){
        snd_sfx_play(sound_file_acept, sound_volume, CENTER);
    }
}

void audio_shutdown() {
    audio_stop_music();
    wav_shutdown();
}
