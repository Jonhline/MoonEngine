#include "plat_sdl.h"
#undef main
#define main night_course_main
#include "night_courses.c"
#undef main
int main(int argc, char **argv)
{
    CHECK(argc>=3);
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    race_view *v = race_view_open(argv[1]);
    CHECK(v);
    CHECK(race_view_request_film(v,"route5","focus_rs_02",0));
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) {
        SDL_Delay(5);
        ready = race_view_poll(v, 0);
    }
    CHECK(ready==1);
    gt4_grid g = gt4_course_grid(&v->now->course, 0);
    float rot[9] = {cosf(g.heading),  0, sinf(g.heading), 0, 1, 0,
                    -sinf(g.heading), 0, cosf(g.heading)};
    race_view_set_pose(v, g.pos, rot);
    float ground = g.pos[1];
    CHECK(gt4_course_surface_triangle(v->now->surface,g.pos[0],g.pos[2],g.pos[1]+5,&ground,NULL,NULL));
    float floor[3] = {g.pos[0], ground, g.pos[2]};
    race_view_set_shadow(v, 0, floor, rot, 1, 1);
    float target[3] = {g.pos[0], g.pos[1] + .3f, g.pos[2]}, cam[3];
    for (int i = 0; i < 3; i++)
        cam[i] = target[i] + rot[i * 3 + 2] * 5 + rot[i * 3] * 2;
    cam[1] += 1.5f;
    r3d_view eye = {0};
    look_at(&eye, cam, target);
    float rows[9];
    for (int i = 0; i < 3; i++) {
        rows[i] = eye.r[i];
        rows[3 + i] = eye.u[i];
        rows[6 + i] = -eye.f[i];
    }
    race_view_set_camera(v, cam, rows, 1.73205f, 2.25378f, .1f);
    fprintf(stderr, "exhaust%u color%g scale%g,%g\n", v->now->car->exhaust_points,
            v->now->afterfire_color[0], v->now->afterfire_scale[0], v->now->afterfire_scale[1]);
    float flames[4][3] = {{.25f, 1, 0}, {.25f, 1, 0}, {.25f, 1, 0}, {.25f, 1, 0}};
    char path[1024];
    snprintf(path, sizeof path, "%s-off.png", argv[2]);
    CHECK(capture(v,path,0));
    uint64_t off = fingerprint;
    race_view_set_afterfire(v, 0, flames, 1);
    snprintf(path, sizeof path, "%s-on.png", argv[2]);
    CHECK(capture(v,path,0));
    CHECK(off!=fingerprint);
    race_view_close(v);
    plat_video_shutdown();
    return 0;
}
