/* Original suggested-gear timing/layout, using the real disc font. */
#include "platform/platform.h"
static int glyph_count;
static int draw_real;
static float glyph_x, glyph_y;
static uint8_t glyph_color[4];
static void record_glyph(const plat_texture *t, float x, float y, float w, float h, float base,
                         float shear, float u0, float v0, float u1, float v1,
                         const uint8_t color[4])
{
    (void)t;
    (void)w;
    (void)h;
    (void)base;
    (void)shear;
    (void)u0;
    (void)v0;
    (void)u1;
    (void)v1;
    glyph_count++;
    glyph_x = x;
    glyph_y = y;
    for (int i = 0; i < 4; i++)
        glyph_color[i] = color[i];
    if (draw_real)
        plat_draw_glyph(t, x, y, w, h, base, shear, u0, v0, u1, v1, color);
}
#define plat_draw_glyph record_glyph
#include "../../src/core/race_hud.c"
#undef plat_draw_glyph
#include <SDL.h>
#include "plat_sdl.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "suggested gear line %d: %s\n", __LINE__, #x);                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    SDL_setenv("MOON_WINDOW_HIDDEN", "1", 1);
    plat_video_desc desc = {640, 480, 0, 0, 0};
    CHECK(!plat_video_init(&desc));
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    race_hud *h = race_hud_open(vol);
    CHECK(h);
    race_hud_state s = {0};
    s.suggested_gear = -1;
    update_suggested_gear(h, &s, 1.f / 60);
    draw_suggested_gear(h, &s);
    CHECK(glyph_count == 0);
    s.suggested_gear = 3;
    for (int i = 0; i < 12; i++)
        update_suggested_gear(h, &s, 1.f / 60);
    CHECK(fabsf(h->suggested_fade - 1) < 1e-6f && h->suggested_last == 3);
    draw_suggested_gear(h, &s);
    CHECK(glyph_count == (argc > 2 ? 0 : 1));
    CHECK(glyph_color[0] == 240 && glyph_color[1] == 0 && glyph_color[2] == 0 && glyph_color[3] == 255);
    float x = glyph_x, y = glyph_y;
    /* 003A7FCC + 003A7108: right edge 640-143, width 44, bottom 464-45+32,
       then text at half-width and bottom-16. Same font alignment as 003A6DE0. */
    hud_font *f = font(h, "GT4meter#1");
    const gt4_glyph *digit = gt4_font_glyph(&f->face, '3');
    CHECK(digit);
    CHECK(fabsf(x - (475 - text_width(f, "3") * .5f + digit->x * f->sx)) < 1e-5f);
    CHECK(fabsf(y - (435 - f->face.height * .5f * f->sy +
                    f->face.baseline * f->sy + digit->y * f->sy)) < 1e-5f);
    draw_gear(h, 3, 0);
    CHECK(fabsf(glyph_x - x - 39) < 1e-5f && fabsf(glyph_y - y + 1) < 1e-5f);
    race_hud_set_widescreen(1);
    draw_suggested_gear(h, &s);
    CHECK(fabsf(glyph_x - x - 96) < 1e-5f && glyph_y == y);
    s.split = 1;
    hud_y_offset = -GT4_SPLIT_HEIGHT;
    draw_suggested_gear(h, &s);
    CHECK(fabsf(glyph_x - x) < 1e-5f && fabsf(glyph_y - y + GT4_SPLIT_HEIGHT) < 1e-5f);
    s.split = 0;
    hud_y_offset = 0;
    race_hud_set_widescreen(0);
    s.panel = 1;
    draw_suggested_gear(h, &s);
    CHECK(fabsf(glyph_x - x + 155) < 1e-5f && fabsf(glyph_y - y + 93) < 1e-5f);
    s.suggested_gear_flash = 1;
    float minimum = 1, maximum = 0;
    for (int i = 0; i < 60; i++) {
        update_suggested_gear(h, &s, 1.f / 60);
        minimum = fminf(minimum, h->suggested_blink.v);
        maximum = fmaxf(maximum, h->suggested_blink.v);
    }
    CHECK(minimum == 0 && maximum > .8f);
    s.suggested_gear = -1;
    update_suggested_gear(h, &s, .1f);
    CHECK(fabsf(h->suggested_fade - .5f) < 1e-6f && h->suggested_last == 3);
    update_suggested_gear(h, &s, .11f);
    glyph_count = 0;
    draw_suggested_gear(h, &s);
    CHECK(glyph_count == 0);
    s.suggested_gear = 2;
    update_suggested_gear(h, &s, .2f);
    s.bspec = 1;
    draw_suggested_gear(h, &s);
    CHECK(glyph_count == 0);
    race_hud_set_course(h, "tsukuba");
    CHECK(h->suggested_last == 0 && h->suggested_fade == 0 && h->suggested_blink.t == 0);
    if (getenv("MOON_SUGGESTED_GEAR_SHOT")) {
        draw_real = 1;
        s = (race_hud_state){0};
        s.gear = 4;
        s.speed = 30;
        s.rpm = 5000;
        s.rev_limit = s.tacho_scale = 8000;
        s.speed_scale = 240;
        s.suggested_gear = 2;
        update_suggested_gear(h, &s, .2f);
        plat_video_begin_frame();
        plat_video_set_view(640, 480);
        draw_simple_panel(h, &s);
        draw_suggested_gear(h, &s);
        int w, height;
        uint8_t *pixels = plat_sdl_read_pixels(&w, &height);
        CHECK(pixels);
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, height, 32, w * 4,
                                                                  SDL_PIXELFORMAT_RGBA32);
        CHECK(surface && !SDL_SaveBMP(surface, getenv("MOON_SUGGESTED_GEAR_SHOT")));
        SDL_FreeSurface(surface);
        free(pixels);
        plat_video_end_frame();
    }
    race_hud_close(h);
    gt4_vol_close(vol);
    plat_video_shutdown();
    puts("Suggested gear: hint, fade, flash, anchors, split, B-Spec and reset passed");
    return 0;
}
