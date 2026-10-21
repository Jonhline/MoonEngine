/* Native enhanced integration: source celestial artwork is retained, and
   directional lighting follows enabled Env0 records, including pre-race. */
#include "plat_sdl.h"
#undef main
#include "../../src/platform/sdl/race_view.c"
#include "miniz.h"
#define main SDL_main
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "authored sky %d: %s\n", __LINE__, #x);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int capture(race_view *v, const char *prefix, const char *name, int pregrid)
{
    plat_video_begin_frame();
    video_begin_scene();
    int ok = pregrid ? race_view_draw_pregrid(v) : race_view_draw(v);
    video_end_scene();
    int w, h;
    size_t size;
    unsigned char *pixels = plat_sdl_read_pixels(&w, &h);
    void *png = pixels ? tdefl_write_image_to_png_file_in_memory(pixels, w, h, 4, &size) : NULL;
    char path[1024];
    snprintf(path, sizeof path, "%s-%s.png", prefix, name);
    FILE *f = png ? fopen(path, "wb") : NULL;
    if (f) {
        ok = ok && fwrite(png, 1, size, f) == size;
        fclose(f);
    } else
        ok = 0;
    free(pixels);
    mz_free(png);
    plat_video_end_frame();
    return ok;
}
int main(int argc, char **argv)
{
    CHECK(argc>=4);
    CHECK(!plat_video_init(&(plat_video_desc){960,720,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    r3d_daylight_options(1, -1, 1);
    r3d_night_sky(1);
    r3d_modern_lighting(1);
    r3d_material_shading(1);
    video_hdr_options(1, 0);
    race_view *v = race_view_open(argv[1]);
    CHECK(v);
    CHECK(race_view_request_film(v,argv[2],"ford_gt_05",0));
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) {
        SDL_Delay(5);
        ready = race_view_poll(v, 0);
    }
    CHECK(ready==1 && v->now);
    race_asset *a = v->now;
    gt4_grid grid = gt4_course_grid(&a->course, 0);
    float rot[9] = {cosf(grid.heading),  0, sinf(grid.heading), 0, 1, 0,
                    -sinf(grid.heading), 0, cosf(grid.heading)};
    race_view_set_pose(v, grid.pos, rot);
    float eye[3] = {grid.pos[0] + 6, grid.pos[1] + 2, grid.pos[2] - 7},
          target[3] = {grid.pos[0], grid.pos[1] + .5f, grid.pos[2]};
    r3d_view c = {0};
    look_at(&c, eye, target);
    float rows[9];
    for (int i = 0; i < 3; i++) {
        rows[i] = c.r[i];
        rows[3 + i] = c.u[i];
        rows[6 + i] = -c.f[i];
    }
    race_view_set_camera(v, eye, rows, 1.73205f, 2.25378f, .1f);
    CHECK(capture(v,argv[3],"pregrid",1));
    if(getenv("AUTHORED_SKY_COMPARE")) {
        r3d_daylight_options(0,-1,0);r3d_modern_lighting(0);r3d_material_shading(0);r3d_night_sky(0);video_hdr_options(0,0);
        CHECK(capture(v,argv[3],"classic",1));
        race_view_close(v);plat_video_shutdown();return 0;
    }

    printf("zone=%d shadow=%d way=%g,%g,%g\n",a->time_zone,a->shadow,a->shadow_way[0],a->shadow_way[1],a->shadow_way[2]);
    CHECK(a->lights.count>0);
    float length = sqrtf(dot3(a->lights.direction[0], a->lights.direction[0]));
    CHECK(length>0);
    float before[3];
    memcpy(before, v->daylight.sun, 12);
    if (argc > 4)
        before[0] += .1f;
    for (int i = 0; i < 3; i++) {
        float want = a->lights.direction[0][i] / length * (a->time_zone ? -1 : 1);
        printf("authored direction %d got=%.9g expected=%.9g\n", i, before[i], want);
        CHECK(fabsf(before[i]-want)<1e-6f);
    }
    race_view_daylight_time(v, 3600);
    CHECK(capture(v,argv[3],"race",0));
    CHECK(!memcmp(before,v->daylight.sun,12));
    CHECK(isfinite(v->daylight.sky[0]) && v->daylight.sky[0]>0);
    race_view_close(v);
    plat_video_shutdown();
    puts("PASS authored direction, finite pregrid sky, fixed course sun/moon at later race time");
    return 0;
}
