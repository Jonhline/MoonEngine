/* P: six native cars, 24 lamp tiles, state/gates and bounded GPU timing. */
#include "render3d.h"
#include "plat_sdl.h"
#include "gl.h"
static unsigned seen_lamps, seen_tiles;
static int omit_depth;
static void observe_lamp_frame(const r3d_car_lamp *lamps, unsigned count)
{
    seen_lamps = count;
    r3d_car_lamp_frame(lamps, count);
}
static int observe_shadow_begin(r3d_lamp_shadow **atlas)
{
    return omit_depth ? 0 : r3d_lamp_shadow_begin(atlas);
}
static int observe_shadow_light(r3d_lamp_shadow *atlas, unsigned index)
{
    int ok = r3d_lamp_shadow_light(atlas, index);
    if (ok)
        seen_tiles |= 1u << index;
    return ok;
}
#define r3d_car_lamp_frame observe_lamp_frame
#define r3d_lamp_shadow_begin observe_shadow_begin
#define r3d_lamp_shadow_light observe_shadow_light
#undef main
#define main night_courses_fixture_main
#include "night_courses.c"
#undef main
#undef r3d_car_lamp_frame
#undef r3d_lamp_shadow_begin
#undef r3d_lamp_shadow_light
static int timed_frame(race_view *v)
{
    seen_tiles = 0;
    plat_video_begin_frame();
    int ok = race_view_draw(v);
    glFinish();
    plat_video_end_frame();
    return ok;
}
int main(int argc, char **argv)
{
    CHECK(argc>=3);
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    video_set_menu_pacing(0);
    video_performance_options(0, 0);
    race_view *v = race_view_open(argv[1]);
    CHECK(v);
    const char *cars[5] = {"focus_rs_02", "focus_rs_02", "focus_rs_02", "focus_rs_02",
                           "focus_rs_02"};
    CHECK(race_view_set_opponents(v,cars,NULL,5));
    CHECK(race_view_request_film(v,"route5","focus_rs_02",0));
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) {
        SDL_Delay(5);
        ready = race_view_poll(v, 0);
    }
    CHECK(ready==1&&v->now);
    gt4_grid first = gt4_course_grid(&v->now->course, 0);
    float rot[9] = {cosf(first.heading),  0, sinf(first.heading), 0, 1, 0,
                    -sinf(first.heading), 0, cosf(first.heading)};
    race_view_set_pose(v, first.pos, rot);
    for (int i = 0; i < 5; i++) {
        gt4_grid grid = gt4_course_grid(&v->now->course, i + 1);
        printf("GRID %d p=%g,%g,%g first=%g,%g,%g\n", i + 1, grid.pos[0], grid.pos[1], grid.pos[2],
               first.pos[0], first.pos[1], first.pos[2]);
        race_view_set_opponent(v, i, grid.pos, rot, NULL);
        CHECK(v->now->lamp_count[i+1]==4);
    }
    r3d_view eye = {0};
    float target[3] = {0}, cam[3];
    for (int entry = 0; entry < 6; entry++)
        for (int k = 0; k < 3; k++)
            target[k] += gt4_course_grid(&v->now->course, entry).pos[k] / 6;
    target[1] += 1;
    for (int k = 0; k < 3; k++)
        cam[k] = target[k] + rot[k * 3 + 2] * -30 + rot[k * 3] * 90;
    cam[1] += 40;
    look_at(&eye, cam, target);
    for (int entry = 0; entry < 6; entry++) {
        gt4_grid grid = gt4_course_grid(&v->now->course, entry);
        float delta[3];
        for (int k = 0; k < 3; k++)
            delta[k] = grid.pos[k] - cam[k];
        float z = dot3(delta, eye.f);
        CHECK(z>0 && fabsf(dot3(delta,eye.r)*1.73205f/z)<.95f && fabsf(dot3(delta,eye.u)*2.25378f/z)<.95f);
    }
    puts("VISIBLE_CARS=6 on the authored 100-metre start grid");
    float rows[9];
    for (int k = 0; k < 3; k++) {
        rows[k] = eye.r[k];
        rows[k + 3] = eye.u[k];
        rows[k + 6] = -eye.f[k];
    }
    race_view_set_camera(v, cam, rows, 1.73205f, 2.25378f, .1f);
    r3d_daylight_options(0, 12, 0);
    r3d_modern_lighting(0);
    r3d_car_shadows(0);
    r3d_shadow_map_quality(0);
    r3d_car_lamps(1);
    CHECK(timed_frame(v));
    car_pose poses[OPPONENTS];
    memcpy(poses, v->other_pose, sizeof poses);
    unsigned char pilots[sizeof v->now->pilot];
    memcpy(pilots, v->now->pilot, sizeof pilots);
    printf("DEVICE %s\n", glGetString(GL_RENDERER));
    for (int mode = 0; mode < 3; mode++) {
        r3d_car_lamps(mode != 0);
        omit_depth = mode == 1;
        for (int i = 0; i < 3; i++)
            CHECK(timed_frame(v));
        r3d_totals before = r3d_work_totals();
        double total = 0, lo = 1e9, hi = 0;
        for (int i = 0; i < 12; i++) {
            uint64_t start = SDL_GetPerformanceCounter();
            CHECK(timed_frame(v));
            double ms =
                (SDL_GetPerformanceCounter() - start) * 1000. / SDL_GetPerformanceFrequency();
            total += ms;
            lo = fmin(lo, ms);
            hi = fmax(hi, ms);
        }
        r3d_totals after = r3d_work_totals();
        CHECK(seen_lamps==(mode?24:0));
        if (mode == 2)
            CHECK(seen_tiles==0xffffffu);
        CHECK(after.readbacks==before.readbacks);
        printf(
            "BENCH mode=%s frames=12 avg_ms=%.3f min_ms=%.3f max_ms=%.3f draws=%.0f triangles=%.0f lamps=%u tiles=%06x\n",
            mode == 0   ? "off"
            : mode == 1 ? "cones"
                        : "occlusion",
            total / 12, lo, hi, (after.draws - before.draws) / 12.,
            (after.triangles - before.triangles) / 12., seen_lamps, seen_tiles);
    }
    CHECK(!memcmp(poses,v->other_pose,sizeof poses)&&!memcmp(pilots,v->now->pilot,sizeof pilots));
    char path[1024];
    snprintf(path, sizeof path, "%s-six-cars.png", argv[2]);
    CHECK(capture(v,path,0));
    /* A time-trial ghost is a visual reference, never a physical light source. */
    race_view_set_ghost(v, 4, .4f);
    if (argc > 3)
        race_view_set_ghost(v, 4, 0);
    update_car_lamps(v, first.pos, rot, 0);
    CHECK(seen_lamps==20);
    race_view_set_ghost(v, 4, 0);
    update_car_lamps(v, first.pos, rot, 0);
    CHECK(seen_lamps==24);
    /* The existing source gates remain independent of the procedural clock. */
    int zone = v->now->time_zone;
    v->now->time_zone = 0;
    update_car_lamps(v, first.pos, rot, 0);
    CHECK(!seen_lamps);
    for (unsigned i = 0; i < 6; i++)
        v->now->pilot[i].brake = 1;
    update_car_lamps(v, first.pos, rot, 0);
    CHECK(seen_lamps==12);
    for (unsigned i = 0; i < 6; i++)
        v->now->pilot[i].brake = 0;
    v->now->time_zone = zone;
    r3d_car_lamps(0);
    update_car_lamps(v, first.pos, rot, 0);
    CHECK(!seen_lamps);
    race_view_close(v);
    plat_video_shutdown();
    puts(
        "PASS six cars / 24 tiles, source gates, no ghost emitters, no render readback or pose writes");
    return 0;
}
