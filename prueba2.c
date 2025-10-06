#include <stdio.h>
#include "wfont.h"
#include "wfont_widths.h"

void print_char_bitmap(unsigned char index) {
    int char_w = wfont_widths[index];
    for(int y = 0; y < WFONT_HEIGHT; y++) {
        for(int x = 0; x < char_w; x++) {
            int byte_index = index * WFONT_CHAR_SIZE + y * WFONT_WIDTH_BYTES + x / 8;
            int bit = (wfont[byte_index] >> (7 - (x % 8))) & 1;
            printf("%c", bit ? '#' : '.');
        }
        printf("\n");
    }
}

int main() {
    printf("Ñ:\n");
    print_char_bitmap(241); // ñ
    printf("\nÑ:\n");
    print_char_bitmap(209); // Ñ
    return 0;
}