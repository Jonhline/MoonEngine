/* Disc-backed watched-car caption; draw boundary is windowless and silent. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
void plat_video_set_view(float w, float h)
{
    (void)w;
    (void)h;
}
static int glyphs;
static void name_glyph(const plat_texture *t, float x, float y, float w, float h, float baseline,
                       float sharpness, float u0, float v0, float u1, float v1, const uint8_t c[4])
{
    (void)baseline;
    (void)sharpness;
    (void)t;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)u0;
    (void)v0;
    (void)u1;
    (void)v1;
    CHECK(c[0]==200 && c[1]==200 && c[2]==200);
    glyphs++;
}
#define advance hud_advance
#define plat_draw_glyph name_glyph
#include "../../src/core/race_hud.c"
#undef plat_draw_glyph
#undef advance
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    race_hud *h = race_hud_open(vol);
    CHECK(h);
    hud_font *f = font(h, "hn_20R#1*90,90");
    CHECK(f && f->ok);
    unsigned enabled = RACE_DISPLAY_CAR_NAME;
    h->fade = .5f;
    race_hud_draw_car_name(h, "Suppressed", 3, 2, enabled, 0, 0);
    CHECK(h->name_fade.value==0 && !h->watched_name[0]);
    h->fade = 1;
    race_hud_draw_car_name(h, "Initial", 0, 0, enabled, 0, 0);
    race_hud_draw_car_name(h, "Initial", 0, 2, enabled, 0, 1.f / 60);
    CHECK(glyphs==0); /* 0039CDF0 does not post a change of entry zero to itself. */
    race_hud_draw_car_name(h, "Nissan", 1, 2, enabled, 0, 1.f / 60);
    CHECK(glyphs==0 && !strcmp(h->watched_name,"Nissan"));
    race_hud_draw_car_name(h, "Nissan", 1, 2, enabled, 0, .125f);
    CHECK(glyphs>0 && h->name_fade.value==.5f);
    glyphs = 0;
    race_hud_draw_car_name(h, "Nissan", 1, 2, 0, 0, .125f);
    CHECK(glyphs==0 && h->name_fade.value==1); /* Hidden captions still age. */
    race_hud_draw_car_name(h, "Nissan", 1, 4, 0, 0, 0);
    CHECK(glyphs>0); /* Original Normal replay selects the name-only word. */
    glyphs = 0;
    for (int i = 0; i < 50; i++)
        race_hud_draw_car_name(h, "Nissan", 1, 2, 0, 0, .125f);
    CHECK(h->name_fade.value==0);
    race_hud_draw_car_name(h, "Nissan", 1, 2, enabled, 0, 0);
    CHECK(glyphs==0);
    race_hud_draw_car_name(h, "Toyota", 2, 2, enabled, 0, 0);
    race_hud_draw_car_name(h, "Toyota", 2, 2, enabled, 0, .25f);
    CHECK(glyphs>0 && !strcmp(h->watched_name,"Toyota"));
    if (argc > 2) {
        glyphs = 0;
        race_hud_draw_car_name(h, "Toyota", 2, 2, 0, 0, 0);
        CHECK(glyphs>0);
    }
    race_hud_close(h);
    gt4_vol_close(vol);
    puts("PASS watched-entry trigger, original caption font, option, replay mode and expiry");
    return 0;
}
