#include "platform/platform.h"
static float glyph_x[128];
static int glyph_count;
static void record_glyph(const plat_texture *texture, float x, float y, float w, float h,
                         float baseline, float shear, float u0, float v0, float u1, float v1,
                         const uint8_t color[4])
{
    (void)texture;
    (void)y;
    (void)w;
    (void)h;
    (void)baseline;
    (void)shear;
    (void)u0;
    (void)v0;
    (void)u1;
    (void)v1;
    (void)color;
    if (glyph_count < 128)
        glyph_x[glyph_count++] = x;
}
#define plat_draw_glyph record_glyph
#include "../../src/core/race_hud.c"
#undef plat_draw_glyph
#include <SDL.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "HUD kerning line %d: %s\n", __LINE__, #x);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    SDL_setenv("OPENGT4_WINDOW_HIDDEN", "1", 1);
    plat_video_desc desc = {320, 240, 0, 0, 0};
    CHECK(!plat_video_init(&desc));
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    race_hud *hud = race_hud_open(vol);
    CHECK(hud);
    hud_font *face = font(hud, "FC_48#1");
    CHECK(face && face->face.contours);
    const char *messages[] = {"START", "FINISH", "TIME UP"};
    const uint8_t white[4] = {255, 255, 255, 255};
    for (int sample = 0; sample < 3; sample++) {
        glyph_count = 0;
        text(face, 100, 100, 0, white, messages[sample]);
        float pen = 100, width = 0;
        unsigned previous = 0;
        int drawn = 0, changed_pairs = 0;
        for (const char *s = messages[sample]; *s; s++) {
            unsigned code = (unsigned char)*s;
            const gt4_glyph *g = gt4_font_glyph(&face->face, code);
            if (!g) {
                pen += face->face.space_advance;
                width += face->face.space_advance;
                previous = 0;
                continue;
            }
            float ink, step = gt4_font_original_step(&face->face, previous, code, 1, &ink);
            if (step != g->advance)
                changed_pairs++;
            if (g->w && g->h) {
                float expected = pen + ink;
                if (argc > 2 && code == 'A')
                    expected = pen + g->x; /* Deliberately discard the T/A kerning. */
                CHECK(drawn < glyph_count && fabsf(glyph_x[drawn] - expected) < .0001f);
                drawn++;
            }
            width += step;
            pen += step;
            previous = code == ' ' ? 0 : code;
        }
        CHECK(drawn == glyph_count && changed_pairs > 0);
        CHECK(fabsf(text_width(face, messages[sample]) - width) < .0001f);
    }
    race_hud_close(hud);
    gt4_vol_close(vol);
    plat_video_shutdown();
    puts("START/FINISH/TIME UP glyph positions and width use the original contour-pair metrics");
    return 0;
}
