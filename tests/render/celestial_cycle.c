/* P: full daily sun/moon arc and actual GPU disc/rotation visibility. */
#include "plat_sdl.h"
#undef main
#define main night_fixture_main
#include "night_sky.c"
#undef main
static int video_started;
static int disc_frame(float hour, int moon, int enabled, int *center, int *ring)
{
    moon_daylight d = moon_night_at(hour);
    const float *f = moon ? d.moon : d.sun;
    float r[3] = {f[2], 0, -f[0]}, len = sqrtf(r[0] * r[0] + r[2] * r[2]);
    for (int k = 0; k < 3; k++)
        r[k] /= len;
    float u[3] = {f[1] * r[2], f[2] * r[0] - f[0] * r[2], -f[1] * r[0]};
    gt4_triangle t[2] = {0};
    const float xy[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
    for (int i = 0; i < 2; i++) {
        t[i].texture = -1;
        t[i].state.depth_func = 1;
        t[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++)
                t[i].v[j].p[k] = f[k] * 3 + r[k] * xy[i * 3 + j][0] + u[k] * xy[i * 3 + j][1];
            t[i].v[j].alpha = 1;
        }
    }
    gt4_model m = {.triangles = t, .count = 2};
    r3d_mesh *mesh = r3d_mesh_create(&m);
    if (!mesh)
        return 0;
    r3d_view v = {0};
    v.rot[0] = v.rot[4] = v.rot[8] = 1;
    memcpy(v.r, r, 12);
    memcpy(v.u, u, 12);
    memcpy(v.f, f, 12);
    v.focal = v.focal_y = 2000;
    v.daylight = 2;
    v.unlit = 1;
    v.color_scale = 1;
    memcpy(v.sun_direction, d.sun, 12);
    memcpy(v.sun_color, d.direct, 12);
    memcpy(v.sky_color, d.sky, 12);
    memcpy(v.haze_color, d.haze, 12);
    r3d_night_sky(enabled);
    plat_video_begin_frame();
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(mesh, &v);
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    r3d_end();
    int w, h;
    uint8_t *p = plat_sdl_read_pixels(&w, &h);
    if (!p) {
        r3d_mesh_destroy(mesh);
        return 0;
    }
    int cx = viewport[0] + viewport[2] / 2, cy = h - 1 - (viewport[1] + viewport[3] / 2);
    *center = 0;
    for (int c = 0; c < 3; c++)
        *center += p[(cy * w + cx) * 4 + c];
    int count = 0;
    *ring = 0;
    for (int y = cy - 22; y < cy + 22; y++)
        for (int x = cx - 22; x < cx + 22; x++)
            if (y >= 0 && x >= 0 && y < h && x < w && (abs(x - cx) > 15 || abs(y - cy) > 15)) {
                for (int c = 0; c < 3; c++)
                    *ring += p[(y * 128 + x) * 4 + c];
                count++;
            }
    *ring /= count;
    free(p);
    plat_video_end_frame();
    r3d_mesh_destroy(mesh);
    return 1;
}
static int run(int argc, char **argv)
{
    r3d_night_sky(1);
    r3d_daylight_options(1, 0, 2);
    for (int i = 0; i <= 1440; i++) {
        double seconds = i;
        float hour =
            r3d_solar_hour(argc > 1 && !strcmp(argv[1], "--clock-negative") ? 0 : seconds, 0);
        float expected = fmodf(i / 60.f, 24);
        CHECK(fabsf(hour-expected)<2e-6f);
        moon_daylight d = moon_night_at(hour), repeat = moon_night_at(hour + 24);
        float norm = 0;
        for (int c = 0; c < 3; c++) {
            CHECK(isfinite(d.moon[c]));
            CHECK(d.moon[c]==-d.sun[c]);
            norm += d.moon[c] * d.moon[c];
            CHECK(fabsf(d.moon[c]-repeat.moon[c])<2e-6f);
        }
        CHECK(fabsf(norm-1)<2e-6f);
    }
    CHECK(moon_night_at(5.99f).sun[1]<0 && moon_night_at(6.01f).sun[1]>0);
    CHECK(moon_night_at(17.99f).moon[1]<0 && moon_night_at(18.01f).moon[1]>0);
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    video_started = 1;
    SDL_HideWindow(plat_sdl_window());
    const float hours[] = {8, 12, 16, 20, 0, 4};
    for (int i = 0; i < 6; i++) {
        int center, ring;
        CHECK(disc_frame(hours[i],i>=3,!(argc>1 && !strcmp(argv[1],"--moon-negative")),&center,&ring));
        printf("%s hour=%g disc=%d surrounding=%d\n", i >= 3 ? "moon" : "sun", hours[i], center,
               ring);
        CHECK(center>ring+20);
    }
    r3d_night_sky(1);
    uint8_t a[65536], b[65536], moved[65536];
    CHECK(sky(0,a));
    CHECK(sky(24,b));
    CHECK(sky(1,moved));
    CHECK(!memcmp(a,b,sizeof a));
    CHECK(memcmp(a,moved,sizeof a));
    puts(
        "PASS full 24h cycle, sunrise/moonrise, sun/moon GPU discs, rotating stars and exact daily repeat");
    return 0;
}
int main(int argc, char **argv)
{
    int result = run(argc, argv);
    if (video_started)
        plat_video_shutdown();
    return result;
}
