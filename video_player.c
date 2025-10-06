// video_player.c
#include <kos.h>
#include <kmg/kmg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_player.h"
#include "audio.h"
#include "sprite.h"
#include "scene.h"

// -----------------------
// DEBUG
// -----------------------
void debug_print_img(const kos_img_t *img)
{
    printf("IMG w=%lu h=%lu byte_count=%lu first16bytes=",
           (unsigned long)img->w, (unsigned long)img->h, (unsigned long)img->byte_count);
    for (int i = 0; i < 16; i++)
        printf("%02X ", ((unsigned char *)img->data)[i]);
    printf("\n");
}

// Función auxiliar para calcular checksum simple
static uint32_t img_checksum(const kos_img_t *img)
{
    uint32_t sum = 0;
    unsigned char *p = img->data;
    for (size_t i = 0; i < img->byte_count; i++)
        sum += p[i];
    return sum;
}

// -----------------------
// HILO DE CARGA (precarga)
// -----------------------
// -----------------------
// HILO DE CARGA (precarga)
// -----------------------
static void *video_loader_thread(void *ptr)
{
    VideoPlayer *vp = (VideoPlayer *)ptr;
    VideoLoader *l = &vp->loader;

    while (1)
    {
        // Esperar a que se solicite un frame
        sem_wait(&l->load_semaphore);

        if (l->file_to_load[0] == '\0')
            continue;

        kos_img_t img;
        printf("video_loader: kmg_to_img %s\n", l->file_to_load);
        if (kmg_to_img(l->file_to_load, &img) != 0)
        {
            printf("video_loader: kmg_to_img fallo para %s\n", l->file_to_load);
            continue;
        }

        // Esperar que el staged anterior sea adoptado
        while (l->staged_ready)
            thd_sleep(1);

        l->staged_img = img;
        l->staged_ready = 1;

        // Notificar al hilo de render
        sem_signal(&l->load_complete);

        // Limpiar request
        l->file_to_load[0] = '\0';
        l->loading = 0;
    }

    return NULL;
}

// -----------------------
// VIDEO REQUEST LOAD
// -----------------------
void video_request_load(VideoPlayer *vp, int frame_num)
{
    VideoLoader *l = &vp->loader;

    if (l->loading || l->file_to_load[0] != '\0')
        return;

    snprintf(l->file_to_load, sizeof(l->file_to_load), "%s/frame%06d.kmg", vp->path, frame_num);
    l->frame_to_load = frame_num;
    l->loading = 1;

    sem_signal(&l->load_semaphore);
}

void video_init(VideoPlayer *vp, const char *path, int frame_count, int fps, const char *audio_file)
{
    vp->frame_count = frame_count;
    vp->current_frame = 0;
    vp->fps = fps;
    vp->frame_duration_ms = (fps > 0) ? (1000 / fps) : 1000;
    vp->finished = 0;
    vp->start_time_ms = timer_ms_gettime64();

    strncpy(vp->path, path, sizeof(vp->path) - 1);
    vp->path[sizeof(vp->path) - 1] = '\0';

    // Cargar primer frame de forma síncrona (para tener algo que dibujar)
    char file[256];
    snprintf(file, sizeof(file), "%s/frame%06d.kmg", vp->path, 1);
    vp->tex = video_load_kmg(file, &vp->w, &vp->h, &vp->tex_w, &vp->tex_h);
    vp->current_frame = 0;

    // Inicializar loader
    VideoLoader *l = &vp->loader;
    l->next_tex = NULL;
    l->next_w = l->next_h = l->next_tex_w = l->next_tex_h = 0;
    l->file_to_load[0] = '\0';
    l->frame_to_load = 0;
    l->loading = 0;

    sem_init(&l->load_semaphore, 0);
    sem_init(&l->load_complete, 0);

    // Crear hilo de loader
    l->loader_thread = thd_create(1, video_loader_thread, vp);

    // Pre-cargar siguiente frame
    if (vp->frame_count > 1)
        video_request_load(vp, 2);

    if (audio_file)
        audio_play_music(audio_file, 0); // sin loop
}

// -----------------------
// VIDEO UPDATE
// -----------------------
void video_update(VideoPlayer *vp)
{
    if (vp->finished)
        return;

    uint32 now = timer_ms_gettime64();
    uint32 elapsed = now - vp->start_time_ms;
    int expected_frame_idx = (int)(elapsed / vp->frame_duration_ms);

    if (expected_frame_idx >= vp->frame_count)
    {
        vp->finished = 1;
        expected_frame_idx = vp->frame_count - 1;
    }

    VideoLoader *l = &vp->loader;

    // Adoptar frame staged si está listo
    if (l->staged_ready)
    {
         // Nuevo log de VRAM

        size_t bytes = (l->staged_img.byte_count > 0) ? l->staged_img.byte_count
                                                      : (size_t)l->staged_img.w * l->staged_img.h * 2;

        kos_img_t img_to_adopt = l->staged_img;
        int loaded_frame = l->frame_to_load;
        l->staged_ready = 0; // Se libera el staged para el loader
        vp->tex = pvr_mem_malloc(bytes);

        if (vp->tex)
        {
            pvr_mem_free(vp->tex);
            kos_img_free(&img_to_adopt, 0);
            vp->w = l->staged_img.w;
            vp->h = l->staged_img.h;
            vp->tex_w = l->staged_img.w;
            vp->tex_h = l->staged_img.h;
            vp->current_frame = loaded_frame;
            memcpy((void *)vp->tex, img_to_adopt.data, bytes);
        }else
        {
            // Si falló VRAM, aún tenemos que liberar la data de KOS
            kos_img_free(&img_to_adopt, 0);
        }

        printf("video_update: adoptada vram=%p frame=%d\n", (void *)vp->tex, loaded_frame);


        l->staged_ready = 0;
        vp->current_frame = l->frame_to_load;
        printf("video_update: adoptada vram=%p frame=%d\n", (void *)vp->tex, loaded_frame); // Nuevo log de VRAM
    }

    // Avanzar current_frame si quedó atrasado
    if (vp->current_frame < expected_frame_idx)
        vp->current_frame = expected_frame_idx;

    // Pedir precarga del siguiente frame
    if (!vp->finished && l->staged_ready == 0 && l->loading == 0)
    {
        int next = vp->current_frame + 1;
        if (next <= vp->frame_count)
            video_request_load(vp, next);
    }
}

void video_draw(VideoPlayer *vp)
{
    if (!vp->tex)
        return;
    printf("video_draw: tex=%p current_frame=%d\n", (void *)vp->tex, vp->current_frame);
    // Fondo negro
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);

    // Dibujar la textura escalada a pantalla completa (640x480)
    video_draw_sprite(0.0f, 0.0f, 640.0f, 480.0f,
                      vp->tex_w, vp->tex_h, vp->tex,
                      PVR_LIST_OP_POLY, 1.0f);
}

void video_shutdown(VideoPlayer *vp)
{
    if (vp->tex)
    {
        pvr_mem_free(vp->tex);
        vp->tex = NULL;
    }
    if (vp->loader.staged_ready)
    {
        kos_img_free(&vp->loader.staged_img, 0);
        vp->loader.staged_ready = 0;
    }

    if (vp->loader.loader_thread)
    {
        thd_destroy(vp->loader.loader_thread);
        vp->loader.loader_thread = NULL;
    }
}

// ---------------------------------
// CARGA SINCRONA (útil para primer frame)
// ---------------------------------
pvr_ptr_t video_load_kmg(const char *filename, uint32 *w, uint32 *h, uint32 *tex_w, uint32 *tex_h)
{
    kos_img_t img;
    if (kmg_to_img(filename, &img) != 0)
    {
        printf("Error cargando %s\n", filename);
        return NULL;
    }
    debug_print_img(&img);
    *w = img.w;
    *h = img.h;

    // tex_w/tex_h asumimos iguales a w/h salvo que tu img tenga campos especificos
    *tex_w = img.w;
    *tex_h = img.h;

    size_t bytes = (img.byte_count > 0) ? img.byte_count : (size_t)img.w * img.h * 2;

    pvr_ptr_t tex = pvr_mem_malloc(bytes);
    if (!tex)
    {
        printf("No hay VRAM para %s\n", filename);
        kos_img_free(&img, 0);
        return NULL;
    }

    memcpy((void *)tex, img.data, bytes);
    kos_img_free(&img, 0);
    return tex;
}

// -------------------------
// Dibuja sprite (igual que tenías)
// -------------------------
void video_draw_sprite(float x, float y, float w, float h,
                       uint32 tex_w, uint32 tex_h, pvr_ptr_t tex,
                       int list, float alpha)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    pvr_poly_cxt_txr(&cxt, list, PVR_TXRFMT_ARGB4444,
                     tex_w, tex_h, tex, PVR_FILTER_BILINEAR);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    // Clamp alpha
    if (alpha < 0.0f)
        alpha = 0.0f;
    if (alpha > 1.0f)
        alpha = 1.0f;
    uint32 argb = ((uint32)(alpha * 255) << 24) | 0x00FFFFFF;

    vert.argb = argb;
    vert.oargb = 0;
    vert.z = 1.0f;
    vert.flags = PVR_CMD_VERTEX;

    // UVs: usamos la textura completa (tex_w/tex_h = tamaño real 256x128)
    vert.x = x;
    vert.y = y;
    vert.u = 0.0f;
    vert.v = 0.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.x = x + w;
    vert.y = y;
    vert.u = 1.0f;
    vert.v = 0.0f;
    pvr_prim(&vert, sizeof(vert));
    vert.x = x;
    vert.y = y + h;
    vert.u = 0.0f;
    vert.v = 1.0f;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = x + w;
    vert.y = y + h;
    vert.u = 1.0f;
    vert.v = 1.0f;
    pvr_prim(&vert, sizeof(vert));
}
