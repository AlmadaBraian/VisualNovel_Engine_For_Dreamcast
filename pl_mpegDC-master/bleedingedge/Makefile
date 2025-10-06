#
# Single file C library for decoding MPEG1 Video and MP2 Audio Dreamcast preliminary port KallistiOS video pvr no sound
#   Ian micheal
KOS_CFLAGS += -std=c99
TARGET = mpegDC.elf
OBJS = mpegDC.o matrix.o

KOS_LOADER = sudo dc-tool-ip -t 192.168.0.118 -c . -x

all: mpegDC.o matrix.o

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS)

rm-elf:
	-rm -f $(TARGET)

mpegDC.o: mpegDC.c
	kos-cc -c mpegDC.c -o mpegDC.o


run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist:
	rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)

