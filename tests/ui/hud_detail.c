/* Original steering bitmap and integrated HUD drawing; no window or audio. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
void plat_video_set_view(float w, float h)
{
    (void)w;
    (void)h;
}
static int wheel_draws;
static float wheel_angles[4];
static const plat_texture *wheel_texture, *g_texture;
static int g_draws;
static void capture_quad(const plat_texture *t, float x, float y, float w, float h, float u0,
                         float v0, float u1, float v1, const uint8_t c[4])
{
    if (t == g_texture)
        g_draws++;
    plat_draw_quad(t, x, y, w, h, u0, v0, u1, v1, c);
}
static void capture_rotated(const plat_texture *t, float x, float y, float w, float h, float cx,
                            float cy, float a, const uint8_t c[4])
{
    if (t != wheel_texture)
        return;
    CHECK(wheel_draws < 4);
    CHECK(x == 235 && y == 365 && w == 170 && h == 170);
    CHECK(cx == 320 && cy == 450);
    CHECK(c[0] == 255 && c[1] == 255 && c[2] == 255 && c[3] == 111);
    wheel_angles[wheel_draws++] = a;
}
#define advance hud_advance
#define plat_draw_quad capture_quad
#define plat_draw_rotated capture_rotated
#include "../../src/core/race_hud.c"
#undef plat_draw_quad
#undef plat_draw_rotated
#undef advance

int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    race_hud *h = race_hud_open(vol);
    CHECK(h && h->img[IMG_STEERING].tex);
    CHECK(h->img[IMG_STEERING].w == 170 && h->img[IMG_STEERING].h == 170);
    wheel_texture = h->img[IMG_STEERING].tex;
    race_hud_state s = {0};
    s.steering_kind = 3;
    light = 1;
    CHECK(!draw_steering(h, &s)); /* Original requires a nonzero input once. */
    s.steering = .5f;
    s.display_hidden = RACE_DISPLAY_METER;
    CHECK(draw_steering(h, &s) && wheel_draws == 4);
    for (int i = 0; i < 4; i++)
        CHECK(wheel_angles[i] == -45);
    wheel_draws = 0;
    s.steering = 0;
    CHECK(draw_steering(h, &s) && wheel_draws == 4);
    for (int i = 0; i < 4; i++)
        CHECK(wheel_angles[i] == -45 + i * 11.25f);
    wheel_draws = 0;
    s.display_hidden |= RACE_DISPLAY_STEERING;
    CHECK(!draw_steering(h, &s) && wheel_draws == 0);
    s.display_hidden = 0;
    s.steering_kind = 2;
    CHECK(!draw_steering(h, &s));
    s.steering_kind = 1;
    CHECK(!draw_steering(h, &s));
    s.steering_kind = 0;
    CHECK(draw_steering(h, &s));
    wheel_draws = 0;
    s.split = 1;
    CHECK(!draw_steering(h, &s) && wheel_draws == 0);
    /* Exercise the public HUD entry point, including G-meter suppression. */
    s.split = 0;
    s.steering_kind = 3;
    s.steering = .5f;
    s.rev_limit = 8000;
    s.gear = 1;
    g_texture = h->img[IMG_G_BASE].tex;
    CHECK(g_texture);
    for (s.panel = 0; s.panel < 2; s.panel++) {
        wheel_draws = g_draws = 0;
        s.display_hidden = 0;
        race_hud_draw(h, &s, 0);
        CHECK(wheel_draws == 4 && g_draws == 0);
        wheel_draws = g_draws = 0;
        s.display_hidden = RACE_DISPLAY_STEERING;
        race_hud_draw(h, &s, 0);
        CHECK(wheel_draws == 0 && g_draws > 0);
        wheel_draws = g_draws = 0;
        s.display_hidden = RACE_DISPLAY_METER;
        race_hud_draw(h, &s, 0);
        CHECK(wheel_draws == 4 && g_draws == 0);
    }
    wheel_draws = 0;
    if (argc > 2) {
        s.split = 0;
        s.display_hidden = RACE_DISPLAY_STEERING;
        CHECK(draw_steering(h, &s)); /* Negative: incorrectly require disabled wheel. */
    }
    race_hud_close(h);
    gt4_vol_close(vol);
    puts("PASS original steering image, geometry, trail and visibility");
    return 0;
}
