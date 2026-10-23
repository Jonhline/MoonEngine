/* The original oil lamp must be lit in both dashboard layouts. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
void plat_video_set_view(float w, float h)
{
    (void)w;
    (void)h;
}
static const plat_texture *oil_texture;
static uint8_t oil_red;
static int oil_draws;
static void oil_quad(const plat_texture *t, float x, float y, float w, float h, float u, float v,
                     float a, float b, const uint8_t c[4])
{
    if (t == oil_texture) {
        oil_red = c[0];
        oil_draws++;
    }
    plat_draw_quad(t, x, y, w, h, u, v, a, b, c);
}
#define advance hud_advance
#define plat_draw_quad oil_quad
#include "../../src/core/race_hud.c"
#undef plat_draw_quad
#undef advance
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    race_hud *h = race_hud_open(vol);
    CHECK(h);
    oil_texture = h->img[IMG_SIGN_OIL].tex;
    CHECK(oil_texture);
    race_hud_state s = {0};
    s.driving = 1;
    s.rev_limit = 8000;
    s.gear = 1;
    s.cars = 1;
    s.rank = 1;
    s.laps = 1;
    s.lap = 1;
    for (s.panel = 0; s.panel < 2; s.panel++) {
        uint8_t red[2];
        for (int on = 0; on < 2; on++) {
            s.oil_sign = on ? 2 : 0;
            if (argc > 2)
                s.oil_sign = 0;
            h->sign_last[2] = 0;
            oil_draws = 0;
            race_hud_draw(h, &s, 0);
            CHECK(oil_draws>0);
            red[on] = oil_red;
        }
        fprintf(stderr, "oil panel%d off/on %u/%u\n", s.panel, red[0], red[1]);
        CHECK(red[1]>red[0]);
    }
    race_hud_close(h);
    gt4_vol_close(vol);
    puts("PASS oil warning switches the original sign in chase and onboard HUDs");
    return 0;
}
