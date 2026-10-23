/* P: real course/car caster integration; all source state remains unchanged. */
#include "render3d.h"
#include "plat_sdl.h"
static int omit_casters;
static void capture_lamp_mesh(r3d_lamp_shadow *atlas, const r3d_mesh *mesh, const r3d_view *view)
{
    if (!omit_casters)
        r3d_lamp_shadow_mesh(atlas, mesh, view);
}
#define r3d_lamp_shadow_mesh capture_lamp_mesh
#undef main
#define main night_courses_fixture_main
#include "night_courses.c"
#undef main
#undef r3d_lamp_shadow_mesh
int main(int argc, char **argv)
{
    if (argc < 3)
        return 1;
    if (plat_video_init(&(plat_video_desc){640, 480, 0, 0, 0}))
        return 1;
    SDL_HideWindow(plat_sdl_window());
    race_view *v = race_view_open(argv[1]);
    CHECK(v);
    CHECK(race_view_request_film(v,"route5","focus_rs_02",0));
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) {
        SDL_Delay(5);
        ready = race_view_poll(v, 0);
    }
    CHECK(ready==1&&v->now);
    gt4_grid grid = gt4_course_grid(&v->now->course, 0);
    float rot[9] = {cosf(grid.heading),  0, sinf(grid.heading), 0, 1, 0,
                    -sinf(grid.heading), 0, cosf(grid.heading)};
    race_view_set_pose(v, grid.pos, rot);
    r3d_view eye = {0};
    float target[3] = {grid.pos[0], grid.pos[1] + .7f, grid.pos[2]}, camera[3];
    for (int k = 0; k < 3; k++)
        camera[k] = grid.pos[k] + rot[k * 3 + 2] * -7.f + rot[k * 3] * 3;
    camera[1] += 2;
    look_at(&eye, camera, target);
    float rows[9];
    for (int k = 0; k < 3; k++) {
        rows[k] = eye.r[k];
        rows[k + 3] = eye.u[k];
        rows[k + 6] = -eye.f[k];
    }
    race_view_set_camera(v, camera, rows, 1.73205f, 2.25378f, .1f);
    r3d_daylight_options(0, 12, 0);
    r3d_modern_lighting(0);
    r3d_car_shadows(0);
    r3d_shadow_map_quality(0);
    r3d_car_lamps(0);
    car_pose pose = v->other_pose[0];
    uint8_t pilot[sizeof v->now->pilot];
    memcpy(pilot, v->now->pilot, sizeof pilot);
    char path[1024];
    snprintf(path, sizeof path, "%s-classic.png", argv[2]);
    CHECK(capture(v,path,0));
    uint64_t original = fingerprint;
    r3d_car_lamps(1);
    omit_casters = 1;
    snprintf(path, sizeof path, "%s-clear.png", argv[2]);
    CHECK(capture(v,path,0));
    uint64_t clear = fingerprint;
    omit_casters = argc > 3;
    snprintf(path, sizeof path, "%s-blocked.png", argv[2]);
    CHECK(capture(v,path,0));
    uint64_t blocked = fingerprint;
    r3d_car_lamps(0);
    snprintf(path, sizeof path, "%s-restored.png", argv[2]);
    CHECK(capture(v,path,0));
    int ok = original == fingerprint && blocked != clear && v->now->lamp_count[0] == 4 &&
             !memcmp(&pose, &v->other_pose[0], sizeof pose) &&
             !memcmp(pilot, v->now->pilot, sizeof pilot);
    printf("native lamp occlusion clear=%llu blocked=%llu Classic=%s state=%s\n",
           (unsigned long long)clear, (unsigned long long)blocked,
           original == fingerprint ? "exact" : "CHANGED",
           !memcmp(pilot, v->now->pilot, sizeof pilot) ? "unchanged" : "CHANGED");
    race_view_close(v);
    plat_video_shutdown();
    return !ok;
}
