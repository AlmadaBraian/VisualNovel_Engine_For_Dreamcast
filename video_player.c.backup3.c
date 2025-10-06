#include <kos.h>
#include "audio.h"
#include <stdio.h>
#include <string.h> // Inicializar video
#include "sprite.h"
#include "scene.h"
#include <png/png.h>
#include "video_player.h"

static void *video_loader_thread(void *ptr)
{
    VideoPlayer *vp = (VideoPlayer *)ptr;
    VideoLoader *l = &vp->loader;

    while (1)
    {
        // Espera hasta que haya algo que cargar
        sem_wait(&l->load_semaphore);

        // Si alguien puso file_to_load, lo cargamos
        if (l->file_to_load[0] != '\0')
        {
            l->next_tex = video_load_texture(l->file_to_load,
                                             &l->next_w,
                                             &l->next_h,
                                             &l->next_tex_w,
                                             &l->next_tex_h);
            // Señalamos que la carga terminó (incrementa contador)
            sem_signal(&l->load_complete);
        }
    }

    return NULL;
}

void video_request_load(VideoPlayer *vp, int frame_num)
{
    VideoLoader *l = &vp->loader;

    // Si ya hay una textura pre-cargada pendiente, no solicitamos otra
    // (mantenemos buffer simple: a lo sumo una next_tex)
    if (l->next_tex != NULL)
        return;

    // preparar nombre y frame_to_load
    snprintf(l->file_to_load, sizeof(l->file_to_load),
             "%s/frame%06d.png", vp->path, frame_num);
    l->frame_to_load = frame_num;

    // despertar hilo (no bloqueante en main)
    sem_signal(&l->load_semaphore);
}

void video_init(VideoPlayer *vp, const char *path, int frame_count, int fps, const char *audio_file)
{
    vp->frame_count = frame_count;
    vp->current_frame = 0;
    vp->fps = fps;
    vp->frame_duration_ms = 1000 / fps;
    vp->finished = 0;
    vp->start_time_ms = timer_ms_gettime64();
    strncpy(vp->path, path, sizeof(vp->path) - 1);
    vp->path[sizeof(vp->path) - 1] = '\0';

    char file[256];
    snprintf(file, sizeof(file), "%s/frame%06d.png", vp->path, 1);
    vp->tex = video_load_texture(file, &vp->w, &vp->h, &vp->tex_w, &vp->tex_h);

    VideoLoader *l = &vp->loader;
    l->next_tex = NULL;
    l->frame_to_load = 0;

    sem_init(&l->load_semaphore, 0);
    sem_init(&l->load_complete, 0);

    l->loader_thread = thd_create(1, video_loader_thread, vp);

    // Pre-cargar el segundo frame
    video_request_load(vp, 2);

    if (audio_file)
    {
        audio_play_music(audio_file, 0); // 0 = no loop
    }
}

void video_update(VideoPlayer *vp)
{
    if (vp->finished)
        return;

    uint32 now = timer_ms_gettime64();
    uint32 elapsed = now - vp->start_time_ms;
    uint32 expected_frame = elapsed / vp->frame_duration_ms;

    if (expected_frame >= vp->frame_count)
    {
        vp->finished = 1;
        expected_frame = vp->frame_count - 1;
    }

    VideoLoader *l = &vp->loader;

    // 1) Si hay textura pre-cargada lista, intentar consumirla sin bloquear
    if (sem_trywait(&l->load_complete) == 0)
    {
        // Tenemos una textura en l->next_tex (puede ser NULL si falló la carga)
        if (l->next_tex)
        {
            // La textura l->next_tex es el archivo (l->frame_to_load).
            // El índice del frame de ese archivo es l->frame_to_load - 1 (ya que el índice 0 es frame0001)
            int loaded_index = l->frame_to_load - 1; // Ej: frame0002 -> loaded_index = 1

            // Solo actualizamos la textura si el frame que acabamos de cargar
            // es igual o más avanzado que el que estamos mostrando (vp->current_frame).
            // Esto previene que se reemplace una textura actual con una vieja cargada tarde.
            if (loaded_index > vp->current_frame)
            {

                // ... (Liberar vp->tex y Adoptar l->next_tex - ESTO ESTÁ BIEN) ...
                if (vp->tex)
                {
                    pvr_mem_free(vp->tex);
                }

                vp->tex = l->next_tex;
                vp->w = l->next_w;
                vp->h = l->next_h;
                vp->tex_w = l->next_tex_w;
                vp->tex_h = l->next_tex_h;

                l->next_tex = NULL;

                // Actualizamos el índice del frame VISIBLE al índice del frame recién cargado.
                vp->current_frame = loaded_index; // <-- ¡CRÍTICO!
            }
            else
            {
                // La carga llegó tarde; el frame ya no se necesita. Liberamos next_tex para no filtrar.
                pvr_mem_free(l->next_tex);
                l->next_tex = NULL;
            }
        }
        // limpiar file_to_load para evitar recarga accidental
        l->file_to_load[0] = '\0';
    }

    // 3) Si no hay textura pre-cargada en cola, pedir la siguiente (current_frame + 2)
    // (notar que mostramos current_frame; el siguiente que queremos tener listo es current_frame+1 -> file N+1 -> frame index+1)
    if (!vp->finished && l->next_tex == NULL) {
    // Calculamos el frame que deberíamos tener listo para el futuro.
    // Usar vp->current_frame + 1 es el estándar (para el siguiente frame).
    // Opcionalmente, puedes intentar la heurística: expected_frame + 1 o expected_frame + 2
    int want = (int)expected_frame + 1; // Pide el frame que toca mostrar en el siguiente tick.

    // Nos aseguramos de no pedir un frame que ya mostramos.
    if (want <= vp->current_frame + 1) {
        want = vp->current_frame + 2; 
    }
    
    if (want <= vp->frame_count) {
        video_request_load(vp, want);
    }
}
}

void video_draw(VideoPlayer *vp)
{
    if (!vp->tex)
        return;

    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    video_draw_sprite(0.0f, 0.0f, 640.0f, 480.0f,
                      vp->tex_w, vp->tex_h, vp->tex,
                      PVR_LIST_OP_POLY, 1.0f);
}

void video_shutdown(VideoPlayer *vp)
{
    if (vp->tex)
        pvr_mem_free(vp->tex);

    if (vp->loader.next_tex)
        pvr_mem_free(vp->loader.next_tex);

    thd_destroy(vp->loader.loader_thread);
}

pvr_ptr_t video_load_texture(const char *filename, uint32 *w, uint32 *h, uint32 *tex_w, uint32 *tex_h)
{
    kos_img_t img;
    if (png_to_img(filename, PNG_FULL_ALPHA, &img) < 0)
    {
        printf("Error cargando %s\n", filename);
        return NULL;
    }

    *w = img.w;
    *h = img.h;
    *tex_w = next_pow2(img.w);
    *tex_h = next_pow2(img.h);

    pvr_ptr_t tex = pvr_mem_malloc(*tex_w * *tex_h * 2);
    if (!tex)
    {
        printf("No se pudo reservar VRAM para %s\n", filename);
        free(img.data);
        return NULL;
    }

    memset(tex, 0, (*tex_w) * (*tex_h) * 2);

    for (uint32 y = 0; y < img.h; y++)
        memcpy((uint16 *)tex + y * (*tex_w), (uint16 *)img.data + y * img.w, img.w * 2);

    free(img.data);
    return tex;
}

void video_draw_sprite(float x, float y, float w, float h,
                       uint32 tex_w, uint32 tex_h, pvr_ptr_t tex,
                       int list, float alpha)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    pvr_poly_cxt_txr(&cxt, list, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED,
                     tex_w, tex_h, tex, PVR_FILTER_BILINEAR);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    // Clamp alpha entre 0 y 1
    if (alpha < 0.0f)
        alpha = 0.0f;
    if (alpha > 1.0f)
        alpha = 1.0f;
    uint32 argb = ((uint32)(alpha * 255) << 24) | 0x00FFFFFF;

    vert.argb = argb;
    vert.oargb = 0;
    vert.z = 1.0f;
    vert.flags = PVR_CMD_VERTEX;

    // Coordenadas UV directas (textura completa)
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
