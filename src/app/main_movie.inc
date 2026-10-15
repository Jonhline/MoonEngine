typedef struct {
    gt4_vol *volume;
    SDL_Thread *thread;
    SDL_mutex *lock;
    SDL_cond *wake;
    sdl_movie *decoder, *pending, *retired, *previous;
    uint64_t switched;
    char name[128], request[128];
    unsigned generation, completed, displayed, finish_notified;
    int stop, interlace, repeat, audio_track, seen, drawn, muted, failed, unavailable;
} movie_view;
static int movie_worker(void *arg)
{
    movie_view *v = arg;
    unsigned handled = 0;
    for (;;) {
        SDL_LockMutex(v->lock);
        while (!v->stop && handled == v->generation && !v->retired)
            SDL_CondWait(v->wake, v->lock);
        sdl_movie *old = v->retired;
        v->retired = NULL;
        if (v->stop) {
            SDL_UnlockMutex(v->lock);
            sdl_movie_close(old);
            break;
        }
        unsigned gen = v->generation;
        char name[128];
        strcpy(name, v->request);
        int interlace = v->interlace, repeat = v->repeat, audio_track = v->audio_track;
        SDL_UnlockMutex(v->lock);
        sdl_movie_close(old);
        if (gen == handled)
            continue;
        sdl_movie *next = NULL;
#ifndef __SWITCH__
        if (*name && v->volume) {
            char path[160];
            size_t size = 0;
            snprintf(path, sizeof path, "/mpeg/main/%s.pss", name);
            uint8_t *data = gt4_vol_read(v->volume, path, &size);
            if (data) {
                next = sdl_movie_open(data, size, interlace, repeat, audio_track);
                free(data);
            }
        }
#endif
        if (!next)
            fprintf(stderr, "movie: cannot load %s; skipping unavailable movie\n", name);
        SDL_LockMutex(v->lock);
        handled = gen;
        if (gen == v->generation) {
            old = v->pending;
            v->pending = next;
            v->completed = gen;
            v->failed = next == NULL;
            next = NULL;
        } else
            old = NULL;
        SDL_UnlockMutex(v->lock);
        sdl_movie_close(old);
        sdl_movie_close(next);
    }
    return 0;
}
static movie_view *layer_movie;
/* The PS2 plays menu movies on one layer of the render context: a movie
   started by a page (TopRoot's start_game) keeps playing under pages pushed
   over it that have no MovieFace, seen through their translucent parts
   (GameRoot's mode selection). Only while the page playing it is still in
   the stack: CourseRoot's course preview must not stay behind MakerRoot,
   which replaced it (startPage). */
#include "menu_movie_layer.h"
static int draw_movie(void *user, const char *name, float x, float y, float w, float h,
                      float opacity, int interlace, int repeat, int audio_track);
static int draw_layer_movie(void *user, const char *name, float x, float y, float w, float h,
                            float opacity, int interlace, int repeat, int audio_track)
{
    movie_view *v = user;
    int muted = v->muted;
    v->muted = 1;
    int result = draw_movie(user, name, x, y, w, h, opacity, interlace, repeat, audio_track);
    v->muted = muted;
    return result;
}
static void movie_layer(menu *m)
{
    /* Pump the requested movie every frame, including decoder promotion and
       retirement. Comparing only its requested name used to stop pumping as
       soon as TopRoot was requested again: the CourseRoot decoder then stayed
       visible forever underneath GameRoot. */
    if (layer_movie)
        menu_movie_layer_draw(m, draw_layer_movie, layer_movie);
}

static void dialog_background(menu *m)
{
    ui_set_scene_renderer(menu_ui(m), draw_pregrid_face, NULL);
    menu_dialog_background_draw(m, draw_layer_movie, layer_movie);
}
static int draw_movie(void *user, const char *name, float x, float y, float w, float h,
                      float opacity, int interlace, int repeat, int audio_track)
{
    movie_view *v = user;
    v->seen = 1;
    if (!v->thread || strlen(name) >= 100 || strchr(name, '/') || strchr(name, '\\')) {
        v->unavailable = 1;
        v->generation = 1;
        return 0;
    }
    SDL_LockMutex(v->lock);
    if (strcmp(v->name, name) || v->interlace != interlace || v->repeat != repeat ||
        v->audio_track != audio_track) {
        sdl_movie_audio(v->decoder, 0);
        sdl_movie_audio(v->previous, 0);
        snprintf(v->name, sizeof v->name, "%s", name);
        strcpy(v->request, v->name);
        v->interlace = interlace;
        v->repeat = repeat;
        v->audio_track = audio_track;
        v->generation++;
        SDL_CondSignal(v->wake);
    }
    v->unavailable = v->completed == v->generation && v->failed;
    if (v->completed == v->generation && v->pending &&
        (sdl_movie_ready(v->pending) || sdl_movie_finished(v->pending)) && !v->retired &&
        !v->previous) {
        v->previous = v->decoder;
        v->decoder = v->pending;
        v->pending = NULL;
        v->displayed = v->generation;
        v->switched = plat_time_ns();
        sdl_movie_audio(v->decoder, !v->muted && audio_track > 0);
    }
    SDL_UnlockMutex(v->lock);
    /* Only a visible MovieFace may enable movie audio. */
    sdl_movie_audio(v->decoder, !v->muted && audio_track > 0);
    float blend = v->previous ? (float)((plat_time_ns() - v->switched) / 180000000.0) : 1;
    if (blend > 1)
        blend = 1;
    if (v->previous)
        sdl_movie_draw(v->previous, x, y, w, h, opacity);
    int drawn = sdl_movie_draw(v->decoder, x, y, w, h, opacity * blend);
    v->drawn = drawn && v->displayed == v->generation && blend >= 1;
    if (v->previous && blend >= 1) {
        SDL_LockMutex(v->lock);
        if (!v->retired) {
            sdl_movie_release_texture(v->previous);
            v->retired = v->previous;
            v->previous = NULL;
            SDL_CondSignal(v->wake);
        }
        SDL_UnlockMutex(v->lock);
    }
    return drawn;
}
