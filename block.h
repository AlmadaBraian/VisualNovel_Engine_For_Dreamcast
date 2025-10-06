// block.h
#ifndef BLOCK_H
#define BLOCK_H

#include <kos.h>

typedef struct {
    float x, y;
    float w, h;
} Block;

void draw_block(Block *b, uint32 color);

#endif
