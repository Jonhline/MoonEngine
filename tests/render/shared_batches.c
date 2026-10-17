/* PC submission invariance, including alpha KEEP and excluded multipass states. */
#include "platform/platform.h"
#include "render3d.h"
#include "video_gl.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sample(int mode, int reference, unsigned char *pixels, r3d_stats *stats)
{
#ifdef _WIN32
    _putenv_s("MOON_RENDER_REFERENCE", reference ? "1" : "");
#else
    if (reference)
        setenv("MOON_RENDER_REFERENCE", "1", 1);
    else
        unsetenv("MOON_RENDER_REFERENCE");
#endif
    gt4_triangle triangles[130] = {0};
    const float positions[3][3] = {{-1, -1, 3}, {1, -1, 3}, {0, 1, 3}};
    for (int i = 0; i < 130; i++) {
        gt4_triangle *t = triangles + i;
        t->texture = -1;
        t->kind = 1;
        t->state.depth_func = 1;
        t->state.alpha_test = mode == 1 || mode == 2;
        t->state.alpha_func = 6;
        t->state.alpha_ref = 64;
        t->state.alpha_fail = mode == 2 ? 1 : 0;
        t->state.destination_test = mode == 3;
        t->blend_enabled = mode == 4;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = t->v + j;
            memcpy(v->p, positions[j], sizeof v->p);
            v->normal[2] = -1;
            v->alpha = (i & 1) ? .8f : .2f;
            v->rgb[0] = (i % 3) ? .6f : .3f;
            v->rgb[1] = .4f;
            v->rgb[2] = .2f;
        }
    }
    gt4_model model = {0};
    model.triangles = triangles;
    model.count = 130;
    r3d_mesh *mesh = r3d_mesh_create(&model);
    if (!mesh)
        return 0;
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    plat_video_begin_frame();
    r3d_begin(0, 0, 640, 448, NULL);
    if (!r3d_begin_target(32))
        return 0;
    r3d_draw(mesh, &view);
    *stats = r3d_frame_stats();
    r3d_end();
    r3d_end_target(pixels);
    plat_video_end_frame();
    r3d_mesh_destroy(mesh);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return 1;
    int negative = argc > 2 && !strcmp(argv[2], "--negative");
    SDL_setenv("MOON_WINDOW_HIDDEN", "1", 1);
    plat_video_desc desc = {320, 240, 0, 0, 0, !strcmp(argv[1], "vulkan")};
    if (plat_video_init(&desc) || !r3d_available())
        return 1;
    video_set_menu_pacing(0);
    int ok = 1;
    for (int mode = 0; mode < 5; mode++) {
        unsigned char optimized[32 * 32 * 4], reference[sizeof optimized];
        r3d_stats a, b;
        if (!sample(mode, negative, optimized, &a) || !sample(mode, 1, reference, &b)) {
            ok = 0;
            break;
        }
        unsigned merged = mode < 2 ? 1 : 0;
        printf("%s mode=%d: draws=%u/%u merged=%u expected=%u\n", argv[1], mode, a.draws, b.draws,
               a.merged_groups, merged);
        if (mode < 2 && !optimized[(16 * 32 + 16) * 4])
            ok = 0;
        if (memcmp(optimized, reference, sizeof optimized) || a.triangles != b.triangles ||
            a.merged_groups != merged || a.draws + merged != b.draws || b.merged_groups)
            ok = 0;
    }
    plat_video_shutdown();
    return ok ? 0 : 1;
}
