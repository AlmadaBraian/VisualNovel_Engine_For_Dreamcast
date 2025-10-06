// font.h
#ifndef FONT_H
#define FONT_H

#include <kos.h>

extern pvr_ptr_t font_tex;
extern float text_scale;
extern pvr_ptr_t font_tex;

void font_init(void);
void draw_char(float x, float y, int c, float scale);
void draw_string(float x, float y, const char *str, int max_chars, int *chars_displayed, float text_scale);

int utf8_next(const char *s, unsigned char *out);


#endif
