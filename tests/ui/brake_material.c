/* PC GPU regression: callback selection must not override material lighting. */
#include <SDL.h>
#include "platform/platform.h"
#include "render3d.h"
#include "plat_sdl.h"
#include "car_model.h"
#include <stdlib.h>
#include "video_gl.h"
#include <stdio.h>
#include <string.h>
static int sample(int kind, int lamp, unsigned char pixels[32 * 32 * 4])
{
    gt4_triangle t[2] = {0};
    const float positions[6][3] = {{-1, -1, 3}, {1, -1, 3}, {1, 1, 3},
                                   {-1, -1, 3}, {1, 1, 3},  {-1, 1, 3}};
    for (int i = 0; i < 2; i++) {
        t[i].texture = -1;
        t[i].kind = kind;
        t[i].lamp = lamp;
        t[i].state.depth_func = 1;
        t[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            memcpy(t[i].v[j].p, positions[i * 3 + j], 12);
            t[i].v[j].normal[2] = -1;
            t[i].v[j].rgb[0] = 1;
            t[i].v[j].alpha = 1;
        }
    }
    gt4_model m = {0};
    m.triangles = t;
    m.count = 2;
    r3d_mesh *mesh = r3d_mesh_create(&m);
    if (!mesh)
        return 0;
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.brake = 1;
    if (!r3d_begin_target(32))
        return 0;
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(mesh, &view);
    r3d_end();
    r3d_end_target(pixels);
    r3d_mesh_destroy(mesh);
    return 1;
}
static int draw_car(const gt4_model *model, unsigned char *pixels)
{
    r3d_mesh *mesh = r3d_mesh_create(model);
    if (!mesh)
        return 0;
    r3d_view view = {0};
    view.rot[0] = view.rot[8] = -1;
    view.rot[4] = 1;
    view.pos[2] = 6;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.brake = 1;
    if (!r3d_begin_target(128)) {
        r3d_mesh_destroy(mesh);
        return 0;
    }
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(mesh, &view);
    r3d_end();
    r3d_end_target(pixels);
    r3d_mesh_destroy(mesh);
    return 1;
}
static int focus(const char *iso)
{
    gt4_vol *vol = gt4_vol_open(iso, NULL);
    if (!vol)
        return 0;
    car_model_tables *tables = car_model_tables_open(vol);
    if (!tables)
        return 0;
    const char *labels[] = {"focus_rs_02", "focus_st170_jp_03"};
    int ok = 1;
    unsigned char pixels[128 * 128 * 4], reference[128 * 128 * 4];
    for (int k = 0; k < 2 && ok; k++) {
        car_model *car = car_model_load_race(vol, tables, labels[k], 0, 6, 0);
        if (!car) {
            ok = 0;
            break;
        }
        gt4_model selected = *car->car;
        selected.count = 0;
        selected.triangles = malloc((size_t)car->car->count * sizeof(gt4_triangle));
        int affected = 0;
        for (int i = 0; i < car->car->count; i++) {
            gt4_triangle t = car->car->triangles[i];
            affected += t.lamp == 2 && t.kind != 0 && t.kind != 5;
            if (t.lamp == 1)
                continue;
            t.lamp = 0;
            selected.triangles[selected.count++] = t;
        }
        ok = affected > 0 && draw_car(car->car, pixels) && draw_car(&selected, reference) &&
             !memcmp(pixels, reference, sizeof pixels);
        printf("Focus brake material %s: %d incorrectly forced-unlit triangles, comparison %s\n",
               labels[k], affected, ok ? "PASS" : "FAIL");
        free(selected.triangles);
        car_model_free(car);
    }
    car_model_tables_close(tables);
    gt4_vol_close(vol);
    return ok;
}
int main(int argc, char **argv)
{
    (void)argv;
    if (plat_video_init(&(plat_video_desc){64, 64, 0, 0, 0}))
        return 2;
    SDL_HideWindow(plat_sdl_window());
    if (!r3d_available())
        return 2;
    unsigned char baseline[4096], brake[4096], unlit[4096];
    if (!sample(1, 0, baseline) || !sample(1, 2, brake) || !sample(0, 2, unlit))
        return 2;
    if (argc > 1 && !strcmp(argv[1], "--negative"))
        brake[2048] ^= 1;
    int ok = !memcmp(baseline, brake, sizeof brake) && memcmp(brake, unlit, sizeof brake);
    if (argc > 1 && strcmp(argv[1], "--negative"))
        ok &= focus(argv[1]);
    plat_video_shutdown();
    if (!ok) {
        fprintf(stderr, "Brake branch replaced authored lighting or negative mutation detected\n");
        return 1;
    }
    puts("PASS brake branch preserves lit material; explicitly unlit material remains brighter");
    return 0;
}
