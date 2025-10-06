#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Nueva función para mapear UTF-8 → índice de fuente y mostrar debug
unsigned char utf8_to_font_index(const char **p) {
    unsigned char c = (unsigned char)**p;

    if (c < 0x80) { // ASCII directo
        (*p)++;
        return c;
    }

    if ((c & 0xE0) == 0xC0) { // 2 bytes UTF-8
        unsigned char c2 = (unsigned char)*(*p + 1);
        unsigned short code = (c << 8) | c2;

        // DEBUG: mostrar qué par de bytes detectamos
        printf("DEBUG: UTF-8 detectado -> c=%02X c2=%02X code=%04X\n", c, c2, code);

        (*p) += 2;

        switch (code) {
            case 0xC3B1:  // ñ
                printf("DEBUG: detectada ñ -> devolviendo índice 164\n");
                return 164;
            case 0xC3A1:  // á
                printf("DEBUG: detectada á -> devolviendo índice 160\n");
                return 160;
            case 0xC3A9:  // é
                printf("DEBUG: detectada é -> devolviendo índice 130\n");
                return 130;
            case 0xC3AD:  // í
                printf("DEBUG: detectada í -> devolviendo índice 161\n");
                return 161;
            case 0xC3B3:  // ó
                printf("DEBUG: detectada ó -> devolviendo índice 162\n");
                return 162;
            case 0xC3BA:  // ú
                printf("DEBUG: detectada ú -> devolviendo índice 163\n");
                return 163;
            default:
                printf("DEBUG: caracter 2 bytes no mapeado, devolviendo '?'\n");
                return '?';
        }
    }

    (*p)++;
    return '?';
}

int main() {
    FILE *f = fopen("escena.json", "rb");
    if (!f) { perror("Error"); return 1; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *data = malloc(size + 1);
    fread(data, 1, size, f);
    data[size] = 0;
    fclose(f);

    // Buscar una línea que contenga ñ para probar
    const char *line = strstr(data, "extra");
    if (!line) { printf("No encontrada\n"); return 1; }

    printf("Probando línea: %s\n", line);

    // Avanzar hasta el primer caracter de la línea
    const char *p = line;
    int i = 0;
    while (*p && *p != '\n') {
        unsigned char ch = utf8_to_font_index(&p);
        printf("Index %d -> ch=%d (0x%02X)\n", i, ch, ch);
        i++;
    }

    free(data);
    return 0;
}
