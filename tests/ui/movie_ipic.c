#include "platform/platform.h"
#include "movie.h"
#include "ipic.h"
#include "vol.h"
#include "menu.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t *uploaded;
static int quads;
static plat_texture *record_create(int w, int h, const uint8_t *pixels)
{
    uploaded = pixels;
    return w == 640 && h == 448 ? (plat_texture *)&uploaded : NULL;
}
static void record_update(plat_texture *texture, const uint8_t *pixels)
{
    (void)texture;
    uploaded = pixels;
}
static void record_quad(const plat_texture *texture, float x, float y, float w, float h, float u0,
                        float v0, float u1, float v1, const uint8_t color[4])
{
    (void)texture;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)u0;
    (void)v0;
    (void)u1;
    (void)v1;
    quads += color[3] == 255;
}
static int draw_pregrid_face(void *user, float x, float y, float w, float h, float opacity)
{
    (void)user;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)opacity;
    return 0;
}
#define plat_texture_create record_create
#define plat_texture_update record_update
#define plat_draw_quad record_quad
#include "../../src/app/main_movie.inc"
#undef plat_texture_create
#undef plat_texture_update
#undef plat_draw_quad

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "IPIC line %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    CHECK(!SDL_Init(SDL_INIT_TIMER));
    gt4_vol *vol = gt4_vol_open_movies(argv[1], NULL);
    CHECK(vol);
    size_t size = 0, stream_size = 0;
    uint8_t *data = gt4_vol_read(vol, "/mpeg/main/course.ipic", &size);
    gt4_ipic archive;
    CHECK(gt4_ipic_open(&archive, data, size));
    CHECK(archive.count == 56);
    CHECK(gt4_ipic_find(&archive, "tsukuba") >= 0);
    CHECK(gt4_ipic_find(&archive, "not_a_movie") == -1);
    CHECK(!gt4_ipic_name(&archive, archive.count));
    gt4_ipic invalid;
    CHECK(!gt4_ipic_open(&invalid, data, 7));
    CHECK(!gt4_ipic_open(&invalid, data, size / 2));
    uint8_t saved[32];
    memcpy(saved, data + 8, 32);
    memset(data + 8, 255, 4);
    CHECK(!gt4_ipic_open(&invalid, data, size));
    memcpy(data + 8, saved, 32);
    memset(data + 12, 'x', 28);
    CHECK(!gt4_ipic_open(&invalid, data, size));
    memcpy(data + 8, saved, 32);
    uint8_t *pixels = sdl_movie_ipic_archive_decode(&archive);
    CHECK(pixels);
    /* Check batch order against independent single-picture decodes. No stored
       game images: every reference comes from the supplied disc. */
    for (unsigned i = 0; i < archive.count; i++) {
        memmove(data + 8, data + 8 + i * 32, 32);
        gt4_ipic one = {data, size, 1};
        uint8_t *stream = gt4_ipic_stream(&one, gt4_ipic_height(&one, 0), &stream_size);
        CHECK(stream);
        uint8_t *reference = sdl_movie_ipic_decode(stream, stream_size, 1);
        free(stream);
        CHECK(reference);
        CHECK(!memcmp(reference, pixels + (size_t)i * 640 * 448 * 4, 640 * 448 * 4));
        free(reference);
    }
    memcpy(data + 8, saved, 32);
    movie_view view = {0};
    view.thread = (SDL_Thread *)&view; /* Deliberately no PSS worker: remains unready. */
    view.lock = SDL_CreateMutex();
    view.wake = SDL_CreateCond();
    view.ipic = archive;
    view.ipic_pixels = pixels;
    CHECK(view.lock && view.wake);
    if (argc > 2)
        view.ipic.count = 0; /* Negative: the old missing-poster path blocks input. */
    for (unsigned step = 0; step < archive.count * 2; step++) {
        unsigned i = step < archive.count ? step : archive.count * 2 - step - 1;
        uploaded = NULL;
        view.poster = NULL; /* Force upload, recorded without renderer allocation. */
        quads = 0;
        const char *name = gt4_ipic_name(&archive, i);
        CHECK(draw_movie(&view, name, 0, 0, 640, 448, 1, 1, 1, 0));
        CHECK(view.drawn && !view.decoder && view.displayed != view.generation);
        CHECK(quads == 1 && uploaded == pixels + (size_t)i * 640 * 448 * 4);
    }
    SDL_DestroyCond(view.wake);
    SDL_DestroyMutex(view.lock);
    free(pixels);
    free(data);
    gt4_vol_close(vol);
    SDL_Quit();
    puts(
        "56 original pictures: independent decodes match; 112 card changes stay ready without PSS");
    return 0;
}
