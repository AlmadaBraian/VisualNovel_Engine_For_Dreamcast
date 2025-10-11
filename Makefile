KOS_BASE ?= C:/DreamSDK2025/DreamSDK/opt/toolchains/dc/kos
KOSLIB = $(KOS_BASE)/lib/dreamcast
MPEG_DIR = pl_mpegDC-master

CC = kos-cc
LD = kos-cc
CFLAGS = -O0 -Wall -g -I. -Ipl_mpegDC-master -Ipl_mpegDC-master/bleedingedge/ -DKOS

# Objetos
OBJS = $(SRCS:.c=.o) main.o font.o sprite.o wfont.o wfont_widths.o scene.o script.o cJSON.o audio.o menu.o video_player.o romdisk.o

# Archivo final
TARGET = juego.elf
KOS_ROMDISK_DIR = romdisk

MUSIC_DIR = music
SOUND_DIR = sound
VIDEO_DIR = video
FRAMES_DIR = frames_kmg
PNG_DIR = png
CD_DIR = cd
CD_ROOT = cd_root
GAME_NAME = "Enlace_Nocturno_v0.06"
GAME_AUTHOR = "La Bacha Soft"
CDI_FILE = $(GAME_NAME).cdi

# --- Phony targets ---
.PHONY: all clean dist copy-music copy-elf cd romdisk

all: clean romdisk.o $(TARGET) copy-resources ipbin cdi

include $(KOS_BASE)/Makefile.rules

# --- Limpiar ---
clean:
	-rm -f $(OBJS) $(TARGET) $(CD_ROOT)/data/* $(CD_ROOT)/1ST_READ.BIN $(CD_ROOT)/IP.BIN $(CDI_FILE)
	-rm -rf $(CD_ROOT)/data
	-rm -f $(TARGET) romdisk.*
	-rm -f $(TARGET)/cd_root/data romdisk.*
	-rm -f $(TARGET)/cd_root/data juego.*

rm-elf:
	-rm -f $(TARGET) romdisk.*


# Compilar ELF
$(TARGET): $(OBJS)
	$(LD)  -o $(TARGET) $(OBJS) -L$(KOSLIB) -lkallisti -lkosutils -lkmg -lpng -lz -lm -lc -lgcc -lwav -lkallisti

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
# Reglas de compilación de los .o
main.o: main.c font.h sprite.h scene.h script.h audio.h menu.h video_player.h
	$(CC) $(CFLAGS) -c main.c
	
sprite.o: sprite.c sprite.h
	$(CC) $(CFLAGS) -c sprite.c

video_player.o: video_player.c video_player.h pl_mpeg.h
	$(CC) $(CFLAGS) -c video_player.c

font.o: font.c font.h sprite.h
	$(CC) $(CFLAGS) -c font.c
	
scene.o: scene.c scene.h cJSON.h
	$(CC) $(CFLAGS) -c scene.c
	
script.o: script.c script.h cJSON.h scene.h audio.h video_player.h
	$(CC) $(CFLAGS) -c script.c
	
cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c
	
menu.o: menu.c menu.h font.h scene.h script.h sprite.h
	$(CC) $(CFLAGS) -c menu.c
	
audio.o: audio.c audio.h
	$(CC) $(CFLAGS) -c audio.c

wfont.o: wfont.h
	$(KOS_BASE)/utils/bin2o/bin2o wfont.h wfont wfont.o
	
wfont_widths.o: wfont_widths.h
	$(KOS_BASE)/utils/bin2o/bin2o wfont_widths.h wfont_widths wfont_widths.o
	
wfont_offsets.o: wfont_offsets.h
	$(KOS_BASE)/utils/bin2o/bin2o wfont_offsets.h wfont_offsets wfont_offsets.o
	
wfont_uvs.o: wfont_uvs.h
	$(KOS_BASE)/utils/bin2o/bin2o wfont_uvs.h wfont_uvs wfont_uvs.o
	
wfont_metrics.o: wfont_metrics.h
	$(KOS_BASE)/utils/bin2o/bin2o wfont_metrics.h wfont_metrics wfont_metrics.o

cd:
	cd $(CD_MAKE_DIR) && make CD_DIR=$(CD_DIR)
	
# --- Convertir ELF a BIN ---
elf2bin: $(TARGET)
	rm -f $(TARGET:.elf=.bin)
	elf2bin $(TARGET)

# --- Scramblear BIN ---
scramble: $(TARGET:.elf=.bin)
	mkdir -p $(CD_ROOT)
	scramble $(TARGET:.elf=.bin) $(CD_ROOT)/1ST_READ.BIN

# --- Copiar ELF y recursos a cd_root ---
copy-resources: $(TARGET)
	@echo "Copiando recursos a $(CD_ROOT)..."
	mkdir -p $(CD_ROOT)/data
	mkdir -p $(CD_ROOT)/music
	mkdir -p $(CD_ROOT)/video
	mkdir -p $(CD_ROOT)/png
	mkdir -p $(CD_ROOT)/sound
	cp $(TARGET) $(CD_ROOT)/data/
	cp -u $(MUSIC_DIR)/*.wav $(CD_ROOT)/music || true
	cp -ur $(VIDEO_DIR)/* $(CD_ROOT)/video || true
	cp -u $(SOUND_DIR)/*.wav $(CD_ROOT)/sound || true
	cp -u $(PNG_DIR)/*.png $(CD_ROOT)/png || true
	cp -u $(KOS_ROMDISK_DIR).o $(CD_ROOT)/data/

# --- Crear IP.BIN ---
ipbin:
	@echo "Generando IP.BIN..."
	cd $(CD_ROOT) && makeip -g $(GAME_NAME) -c $(GAME_AUTHOR) -f IP.BIN

# --- Generar CDI ---
cdi:
	@echo "Generando CDI con mkdcdisc..."
	$(KOS_BASE)/utils/mkdcdisc/builddir/mkdcdisc \
		-e $(CD_ROOT)/data/$(TARGET) \
		-p $(CD_ROOT)/IP.BIN \
		-d $(CD_ROOT)/music \
		-d $(CD_ROOT)/video \
		-d $(CD_ROOT)/sound \
		-d $(CD_ROOT)/png \
		-f $(CD_ROOT)/video_audio.wav \
		-f $(CD_ROOT)/intro.mpg \
		-o $(CDI_FILE) \
		-n $(GAME_NAME) \
		-a $(GAME_AUTHOR)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS) romdisk.img
	$(KOS_STRIP) $(TARGET)
