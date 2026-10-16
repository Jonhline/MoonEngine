/* Exercise authored specular-only layers through the PC GPU renderer. */
#include "platform/platform.h"
#include "render3d.h"
#include "video_gl.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sample(int kind, float specular, float vertex_limit, int reflection,
                  unsigned char pixel[4])
{
    gt4_triangle triangles[2] = {0};
    const float positions[6][3] = {{-1, -1, 3}, {1, -1, 3}, {1, 1, 3},
                                   {-1, -1, 3}, {1, 1, 3},  {-1, 1, 3}};
    for (int i = 0; i < 2; i++) {
        triangles[i].texture = reflection ? -2 : -1;
        triangles[i].kind = kind;
        triangles[i].state.depth_func = 1;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &triangles[i].v[j];
            memcpy(v->p, positions[i * 3 + j], sizeof v->p);
            v->normal[2] = -1;
            v->alpha = 1;
            v->shine = 8;
            for (int c = 0; c < 3; c++) {
                v->rgb[c] = reflection ? .2f * reflection : .8f;
                v->ambient[c] = .5f;
                v->specular[c] = specular;
            }
        }
    }
    gt4_model model = {0};
    model.triangles = triangles;
    model.count = 2;
    r3d_mesh *mesh = r3d_mesh_create(&model);
    if (!mesh)
        return 0;
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    r3d_lights lights = {0};
    lights.count = 1;
    lights.direction[0][2] = -1;
    lights.vertex_limit = vertex_limit;
    lights.specular_scale = 1;
    for (int c = 0; c < 3; c++) {
        lights.ambient[0][c] = .2f;
        lights.diffuse[0][c] = .25f;
        lights.specular[0][c] = .5f;
    }
    unsigned char white[] = {255, 255, 255, 128};
    tex1_image environment = {0};
    environment.width = environment.height = 1;
    environment.rgba = white;
    r3d_environment_changed();
    r3d_begin(0, 0, 640, 448, &environment);
    if (!r3d_begin_target(32)) {
        r3d_mesh_destroy(mesh);
        return 0;
    }
    r3d_draw_lit(mesh, &view, &lights);
    r3d_end();
    unsigned char pixels[32 * 32 * 4];
    r3d_end_target(pixels);
    memcpy(pixel, pixels + (16 * 32 + 16) * 4, 4);
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
    plat_video_set_view(320, 240);
    video_set_menu_pacing(0);
    plat_video_begin_frame();
    /* Original: func_0049EB90 +0x84..0x94 loads only the two specular
       colour vectors. Ambient and diffuse must not enter this layer.
       N.H = 1 here, so every original exponent gives the same result. */
    const float specular[] = {0, .25f, 1};
    int ok = 1;
    for (int race = 0; race < 2; race++) {
        for (int kind = 1; kind <= 4; kind += 3) {
            for (int i = 0; i < 3; i++) {
                unsigned char pixel[4];
                float expected = specular[i] * .5f + (kind == 4 ? 0 : .3f);
                float material = negative && i == 1 ? 1 : specular[i];
                if (!sample(kind, material, race ? 255.f / 128 : 0, 0, pixel)) {
                    ok = 0;
                    continue;
                }
                int level = (int)(expected * 255 + .5f);
                printf("%s race=%d kind=%d specular=%g: %u, expected %d\n", argv[1], race, kind,
                       specular[i], pixel[0], level);
                for (int c = 0; c < 3; c++)
                    if (abs(pixel[c] - level) > 1)
                        ok = 0;
            }
        }
    }
    /* VU1 0x215 uses the authored reflection colour, without the
       ambient/diffuse/specular light blocks used by the surface pass. */
    for (int strength = 1; strength <= 3; strength++) {
        for (int i = 0; i < 3; i++) {
            unsigned char pixel[4];
            int level = 51 * strength;
            if (!sample(1, specular[i], 0, strength, pixel)) {
                ok = 0;
                continue;
            }
            printf("%s reflection=%g specular=%g: %u, expected %d\n", argv[1], .2f * strength,
                   specular[i], pixel[0], level);
            for (int c = 0; c < 3; c++)
                if (abs(pixel[c] - level) > 1)
                    ok = 0;
        }
    }
    plat_video_shutdown();
    return ok ? 0 : 1;
}
