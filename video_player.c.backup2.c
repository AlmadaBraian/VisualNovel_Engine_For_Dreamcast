// video_player.c
#include "video_player.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png/png.h>

// --- cargar PNG en textura PVR ---
static pvr_ptr_t load_png_to_pvr(const char *filename, uint32 *w, uint32 *h)
{
    kos_img_t img;
    if (png_to_img(filename, PNG_FULL_ALPHA, &img) < 0)
    {
        printf("Error cargando PNG: %s\n", filename);
        return 0;
    }
    //printf("Cargando PNG: %s\n", filename);

    *w = img.w;
    *h = img.h;

    pvr_ptr_t tex = pvr_mem_malloc(img.w * img.h * 2);
    if (tex)
        pvr_txr_load(img.data, tex, img.w * img.h * 2);

    free(img.data);
    return tex;
}

// --- cargar frame en slot ---
static void load_frame_to_slot(VideoPlayer *vp, int frame_idx, FrameSlot *slot)
{
    if (!slot) return;

    // si el slot ya tiene este frame, no hacemos nada
    if (slot->valid && slot->frame_index == frame_idx)
        return;

    // liberar memoria si hay algo
    if (slot->valid) {
        pvr_mem_free(slot->tex);
        slot->valid = 0;
    }

    // validar índices
    if (frame_idx < 0 || frame_idx >= vp->frame_count) {
        slot->valid = 0;
        return;
    }

    uint16 name_idx = vp->frame_indices[frame_idx];
    if (name_idx >= (uint16)vp->name_count) {
        printf("Nombre inválido index %u para frame %d\n", name_idx, frame_idx);
        slot->valid = 0;
        return;
    }

    char filename[MAX_PATH_LEN];
    //snprintf(filename, sizeof(filename), "%s/%s", vp->path, vp->frame_names[name_idx]);

    slot->tex = load_png_to_pvr(filename, &slot->w, &slot->h);
    slot->valid = (slot->tex != 0);
    slot->frame_index = frame_idx;

    if (!slot->valid) {
        printf("Fallo al cargar frame %d (%s)\n", frame_idx, filename);
    }
}

void video_init(VideoPlayer *vp, const char *path, int fps,
                const char *bin_file, const char *names_file,
                const char *audio_file)
{
    vp->fps = fps;
    vp->frame_duration_ms = (fps > 0) ? (1000 / fps) : 0;
    vp->elapsed_ms = 0;
    vp->finished = 0;

    strncpy(vp->path, path, sizeof(vp->path) - 1);
    vp->path[sizeof(vp->path) - 1] = 0;

    vp->buffer_start = 0;
    vp->buffer_end = 0;
    vp->buffer_count = 0;      // IMPORTANT
    vp->current_index = 0;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        vp->buffer[i].valid = 0;
        vp->buffer[i].frame_index = -1;
        vp->buffer[i].tex = NULL;
    }

    // Cargar frames y nombres
    if (video_load_frames(vp, bin_file, names_file) < 0) {
        printf("video_init: error cargando frame list\n");
        return;
    }

    // Precargar solo hasta BUFFER_SIZE (no más)
    int to_load = (vp->frame_count < BUFFER_SIZE) ? vp->frame_count : BUFFER_SIZE;
    for (int i = 0; i < to_load; i++) {
        load_frame_to_slot(vp, i, &vp->buffer[i]);
        vp->buffer_count++;
    }
    vp->buffer_start = 0;
    vp->buffer_end = vp->buffer_count % BUFFER_SIZE;

    // Reproducir audio y comprobar resultado
    if (audio_file) {
        audio_play_music(audio_file, 0); // si tu audio_play_music devuelve handle, guárdalo o comprueba
    }
}

int video_load_frames(VideoPlayer *vp, const char *bin_file, const char *names_file)
{
    // Abrir bin con tiempos + índices
    FILE *fb = fopen(bin_file, "rb");
    if (!fb)
    {
        printf("No se pudo abrir bin: %s\n", bin_file);
        return -1;
    }

    fseek(fb, 0, SEEK_END);
    long bin_size = ftell(fb);
    fseek(fb, 0, SEEK_SET);

    int frame_count = bin_size / (sizeof(uint32_t) + sizeof(uint16_t));
    vp->frame_count = frame_count;

    vp->frame_times = malloc(sizeof(uint32_t) * frame_count);
    vp->frame_indices = malloc(sizeof(uint16_t) * frame_count);
    if (!vp->frame_times || !vp->frame_indices)
    {
        printf("No hay memoria para frames\n");
        fclose(fb);
        return -1;
    }

    for (int i = 0; i < frame_count; i++)
    {
        uint32_t t;
        uint16_t idx;
        fread(&t, sizeof(uint32_t), 1, fb);
        fread(&idx, sizeof(uint16_t), 1, fb);
        vp->frame_times[i] = t;
        vp->frame_indices[i] = idx;
    }
    fclose(fb);

    // Abrir TXT con nombres
    FILE *fn = fopen(names_file, "r");
    if (!fn)
    {
        printf("No se pudo abrir txt: %s\n", names_file);
        return -1;
    }

    int name_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fn))
        name_count++;
    rewind(fn);

    vp->frame_names = malloc(sizeof(char *) * name_count);
    vp->name_count = name_count;

    for (int i = 0; i < name_count; i++)
    {
        if (fgets(line, sizeof(line), fn))
        {
            line[strcspn(line, "\r\n")] = 0;
            vp->frame_names[i] = strdup(line);
        }
        else
        {
            vp->frame_names[i] = strdup("frame_missing.png");
        }
    }
    fclose(fn);

    //printf("Frames cargados: %d, Nombres: %d\n", frame_count, name_count);
    return 0;
}

// --- actualizar video ---
void video_update(VideoPlayer *vp, int delta_ms)
{
    if (!vp || vp->finished) return;

    // proteger contra deltas enormes
    if (delta_ms < 0) delta_ms = 0;
    if (delta_ms > 500) delta_ms = 500;

    vp->elapsed_ms += delta_ms;

    // avanzar mientras toca
    while (vp->current_index < vp->frame_count &&
           vp->elapsed_ms >= (int)vp->frame_times[vp->current_index])
    {
        int frame_idx = vp->current_index;

        // --- ¿está ya en el buffer? ---
        int found = -1;
        for (int i = 0; i < vp->buffer_count; i++) {
            int slot_i = (vp->buffer_start + i) % BUFFER_SIZE;
            if (vp->buffer[slot_i].valid && vp->buffer[slot_i].frame_index == frame_idx) {
                found = slot_i;
                break;
            }
        }

        // --- si no está, cargamos reemplazando el slot más viejo ---
        if (found == -1) {
            FrameSlot *slot;
            if (vp->buffer_count < BUFFER_SIZE) {
                // aún no llenamos el buffer, usar buffer_end
                slot = &vp->buffer[vp->buffer_end];
                vp->buffer_end = (vp->buffer_end + 1) % BUFFER_SIZE;
                vp->buffer_count++;
            } else {
                // buffer lleno: reemplazamos buffer_start (slot más viejo)
                slot = &vp->buffer[vp->buffer_start];
                // liberar contenido viejo
                if (slot->valid) {
                    pvr_mem_free(slot->tex);
                    slot->valid = 0;
                }
                vp->buffer_start = (vp->buffer_start + 1) % BUFFER_SIZE;
                vp->buffer_end = (vp->buffer_end + 1) % BUFFER_SIZE;
            }

            load_frame_to_slot(vp, frame_idx, slot);
        }

        // avanzamos al siguiente frame
        vp->current_index++;
    }

    if (vp->current_index >= vp->frame_count)
        vp->finished = 1;
}


// --- dibujar ---
void video_draw(VideoPlayer *vp)
{
    if (!vp || vp->finished || vp->current_index == 0)
        return;

    int cur_frame_idx = vp->current_index - 1;
    FrameSlot *slot = NULL;

    // buscar slot con el frame actual entre los slots cargados
    for (int i = 0; i < vp->buffer_count; i++)
    {
        int b = (vp->buffer_start + i) % BUFFER_SIZE;
        if (vp->buffer[b].valid && vp->buffer[b].frame_index == cur_frame_idx)
        {
            slot = &vp->buffer[b];
            break;
        }
    }

    if (!slot || !slot->valid) {
        // debug: el frame actual no está en buffer (posible que llegues demasiado atrás)
        // printf("Frame %d no encontrado en buffer\n", cur_frame_idx);
        return;
    }

    // dibujar full screen
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                     PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED,
                     slot->w, slot->h, slot->tex,
                     PVR_FILTER_BILINEAR);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    pvr_vertex_t v[4];
    v[0].x = 0;   v[0].y = 0;   v[0].z = 1; v[0].u = 0; v[0].v = 0; v[0].argb = 0xFFFFFFFF; v[0].flags = PVR_CMD_VERTEX;
    v[1].x = 640; v[1].y = 0;   v[1].z = 1; v[1].u = 1; v[1].v = 0; v[1].argb = 0xFFFFFFFF; v[1].flags = PVR_CMD_VERTEX;
    v[2].x = 0;   v[2].y = 480; v[2].z = 1; v[2].u = 0; v[2].v = 1; v[2].argb = 0xFFFFFFFF; v[2].flags = PVR_CMD_VERTEX;
    v[3].x = 640; v[3].y = 480; v[3].z = 1; v[3].u = 1; v[3].v = 1; v[3].argb = 0xFFFFFFFF; v[3].flags = PVR_CMD_VERTEX_EOL;

    for (int i = 0; i < 4; i++)
        pvr_prim(&v[i], sizeof(pvr_vertex_t));
}

// --- liberar memoria ---
void video_shutdown(VideoPlayer *vp)
{
    if (!vp)
        return;

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        if (vp->buffer[i].valid)
        {
            pvr_mem_free(vp->buffer[i].tex);
            vp->buffer[i].valid = 0;
        }
    }

    if (vp->frame_times)
        free(vp->frame_times);
    if (vp->frame_indices)
        free(vp->frame_indices);
    if (vp->frame_names)
    {
        for (int i = 0; i < vp->name_count; i++)
            free(vp->frame_names[i]);
        free(vp->frame_names);
    }

    vp->frame_count = 0;
    vp->current_index = 0;
    vp->elapsed_ms = 0;
    vp->finished = 0;
}
