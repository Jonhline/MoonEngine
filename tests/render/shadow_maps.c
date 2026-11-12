/* P: geometric projection union, opacity, receiver depth and reversible state. */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "render3d.h"
#include "video_gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "car shadow %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static r3d_mesh *rectangle(float x0, float x1, float z0, float z1, float height, int red,
                           float slope)
{
    gt4_triangle t[2] = {0};
    const float points[6][3] = {{x0, height, z0}, {x1, height, z0}, {x1, height, z1},
                                {x0, height, z0}, {x1, height, z1}, {x0, height, z1}};
    for (int i = 0; i < 2; i++) {
        t[i].texture = -1;
        t[i].state.depth_func = 1;
        t[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            memcpy(t[i].v[j].p, points[i * 3 + j], sizeof points[0]);
            t[i].v[j].p[1] += slope * t[i].v[j].p[0];
            t[i].v[j].rgb[0] = 1;
            t[i].v[j].rgb[1] = t[i].v[j].rgb[2] = red ? 0 : 1;
            t[i].v[j].alpha = 1;
        }
    }
    gt4_model model = {0};
    model.triangles = t;
    model.count = 2;
    uint8_t pixels[8 * 8 * 4], alpha[8 * 8];
    tex1_image texture = {.width = 8,
                          .height = 8,
                          .tex_w = 8,
                          .tex_h = 8,
                          .rgba = pixels,
                          .native_alpha = alpha,
                          .color_component = 1,
                          .texture_function = 0};
    if (red == 2) {
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                int at = y * 8 + x;
                pixels[at * 4] = pixels[at * 4 + 1] = pixels[at * 4 + 2] = 255;
                pixels[at * 4 + 3] = 255;
                alpha[at] = x < 4 ? 0 : 128;
            }
        for (int i = 0; i < 2; i++) {
            t[i].texture = 0;
            t[i].state.alpha_test = 1;
            t[i].state.alpha_func = 6;
            t[i].state.alpha_ref = 64;
            for (int j = 0; j < 3; j++) {
                t[i].v[j].uv[0] = (t[i].v[j].p[0] - x0) / (x1 - x0);
                t[i].v[j].uv[1] = (t[i].v[j].p[2] - z0) / (z1 - z0);
            }
        }
        model.textures = &texture;
        model.ntextures = 1;
    }
    if (red == 3)
        for (int i = 0; i < 2; i++)
            t[i].blend_enabled = 1;
    return r3d_mesh_create(&model);
}

static int frame(r3d_shadow_map **map, r3d_mesh *floor, r3d_mesh *caster, r3d_mesh *receiver,
                 int quality, int duplicate, float offset, float opacity, uint8_t *pixels,
                 int daylight)
{
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.cam[1] = 5;
    view.cam[2] = -6;
    view.r[0] = 1;
    view.f[1] = -.4472136f;
    view.f[2] = .8944272f;
    view.u[1] = .8944272f;
    view.u[2] = .4472136f;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    view.daylight = daylight;
    view.sun_direction[1] = 1;
    for (int c = 0; c < 3; c++) {
        view.sun_color[c] = 1;
        view.sky_color[c] = 1;
    }
    r3d_daylight_options(daylight, 12, 0);
    if (!r3d_begin_target(128))
        return 0;
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_shadow_map_quality(quality);
    r3d_shadow_map_bind(NULL);
    if (quality) {
        const float centre[3] = {0, 0, 4},
                    light[3] = {daylight ? 0.f : .8f, -1, daylight ? 0.f : .3f};
        if (!r3d_shadow_map_begin(map, centre, light))
            return 0;
        r3d_view posed = view;
        posed.pos[0] = offset;
        r3d_shadow_map_mesh(*map, floor, &view, opacity);
        if (receiver)
            r3d_shadow_map_mesh(*map, receiver, &view, opacity);
        r3d_shadow_map_mesh(*map, caster, &posed, opacity);
        if (duplicate)
            r3d_shadow_map_mesh(*map, caster, &posed, opacity);
        r3d_shadow_map_end(*map);
        r3d_shadow_map_bind(*map);
    }
    glClearColor(.25f, .25f, .25f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r3d_draw(floor, &view);
    if (receiver)
        r3d_draw(receiver, &view);
    r3d_end();
    r3d_shadow_map_bind(NULL);
    r3d_end_target(pixels);
    return 1;
}

int main(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    r3d_mesh *floor = rectangle(-12, 12, -1, 20, 0, 0, 0);
    r3d_mesh *caster = rectangle(-1, 1, 3, 5, 1, 0, 0);
    r3d_mesh *body = rectangle(-3, 3, 2, 6, .5f, 1, 0);
    r3d_mesh *slope = rectangle(-12, 12, -1, 20, 0, 0, .25f);
    r3d_mesh *cutout = rectangle(-1, 1, 3, 5, 1, 2, 0);
    r3d_mesh *glass = rectangle(-1, 1, 3, 5, 1, 3, 0);
    CHECK(floor && caster && body && slope && cutout && glass);
    r3d_shadow_map *map = NULL;
    uint8_t base[65536], shadow[65536], twice[65536], zero[65536], moved[65536], front[65536],
        front_shadow[65536];
    CHECK(frame(&map,floor,caster,NULL,0,0,0,.6f,base,0));
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    for (int level = 1; level <= 3; level++) {
        CHECK(frame(&map,floor,caster,NULL,negative?0:level,0,0,.6f,shadow,0));
        int changed = 0, soft = 0;
        for (int i = 0; i < 128 * 128; i++) {
            changed += shadow[i * 4] < base[i * 4];
            soft += base[i * 4] > 80 && shadow[i * 4] > base[i * 4] * .5f &&
                    shadow[i * 4] < base[i * 4] * .95f;
        }
        printf("map=%d shadow pixels=%d soft-edge=%d\n", r3d_shadow_map_size(), changed, soft);
        CHECK(changed>30 && soft>0);
        unsigned char levels[256] = {0};
        int gradients = 0;
        for (int pixel = 0; pixel < 128 * 128; pixel++)
            if (base[pixel * 4] > 100 && shadow[pixel * 4] > base[pixel * 4] * .4f &&
                shadow[pixel * 4] < base[pixel * 4] * .98f)
                levels[shadow[pixel * 4]] = 1;
        for (int value = 0; value < 256; value++)
            gradients += levels[value];
        printf("continuous penumbra levels=%d\n", gradients);
        CHECK(gradients>=8);
        CHECK(r3d_shadow_map_size()==(256<<level));
        CHECK(frame(&map,floor,caster,NULL,level,1,0,.6f,twice,0));
        CHECK(!memcmp(shadow,twice,sizeof shadow));
        CHECK(frame(&map,floor,caster,NULL,level,0,0,0,zero,0));
        CHECK(!memcmp(base,zero,sizeof base));
        CHECK(frame(&map,floor,caster,NULL,level,0,2,.6f,moved,0));
        CHECK(memcmp(shadow,moved,sizeof shadow));
        CHECK(frame(&map,floor,caster,body,0,0,0,.6f,front,0));
        CHECK(frame(&map,floor,caster,body,level,0,0,.6f,front_shadow,0));
        int body_shadow = 0;
        for (int i = 0; i < 128 * 128; i++)
            body_shadow +=
                front[i * 4] > 80 && front[i * 4 + 1] < 5 && front_shadow[i * 4] < front[i * 4];
        printf("body shadow pixels=%d\n", body_shadow);
        CHECK(body_shadow>30);
        CHECK(frame(&map,floor,NULL,NULL,level,0,0,.6f,zero,0));
        CHECK(!memcmp(base,zero,sizeof base));
        CHECK(frame(&map,slope,NULL,NULL,0,0,0,.6f,front,0));
        CHECK(frame(&map,slope,NULL,NULL,level,0,0,.6f,front_shadow,0));
        CHECK(!memcmp(front,front_shadow,sizeof front));
        CHECK(frame(&map,slope,caster,NULL,level,0,0,.6f,front_shadow,0));
        CHECK(memcmp(front,front_shadow,sizeof front));
        CHECK(frame(&map,floor,glass,NULL,level,0,0,.6f,zero,0));
        CHECK(!memcmp(base,zero,sizeof base));
        CHECK(frame(&map,floor,cutout,NULL,level,0,0,.6f,zero,0));
        int opaque_energy = 0, cutout_energy = 0;
        for (int i = 0; i < 128 * 128; i++) {
            opaque_energy += base[i * 4] - shadow[i * 4];
            cutout_energy += base[i * 4] - zero[i * 4];
        }
        printf("opaque energy=%d cutout=%d; flat/sloped self surfaces unchanged\n", opaque_energy,
               cutout_energy);
        CHECK(cutout_energy>opaque_energy*.25 && cutout_energy<opaque_energy*.8);
    }
    CHECK(frame(&map,floor,caster,NULL,0,0,0,.6f,zero,0));
    CHECK(!memcmp(base,zero,sizeof base));
    /* P regression: strong contact under an opaque raised caster, rather than
       direct-only attenuation washed out by the sky fill. The negative removes
       the map and must fail the same minimum-darkening requirement. */
    CHECK(frame(&map,floor,caster,NULL,0,0,0,1,base,1));
    CHECK(frame(&map,floor,caster,NULL,(argc>1 && !strcmp(argv[1],"--contact-negative"))?0:3,0,0,1,shadow,1));
    int contact_pixels = 0;
    float strongest = 1;
    for (int i = 0; i < 128 * 128; i++) {
        if (base[i * 4] < 100)
            continue;
        float ratio = (float)shadow[i * 4] / base[i * 4];
        if (ratio < strongest)
            strongest = ratio;
        contact_pixels += ratio < .7f;
    }
    printf("daylight contact: pixels=%d minimum ratio=%.3f\n", contact_pixels, strongest);
    CHECK(contact_pixels>20 && strongest<.7f);
    CHECK(frame(&map,floor,NULL,NULL,3,0,0,1,zero,1));
    CHECK(!memcmp(base,zero,sizeof base));
    r3d_daylight_options(0, 12, 0);
    r3d_shadow_map_quality(99);
    CHECK(!r3d_shadow_map_size());
    r3d_shadow_map_quality(-1);
    CHECK(!r3d_shadow_map_size());
    r3d_shadow_map_destroy(map);
    r3d_mesh_destroy(floor);
    r3d_mesh_destroy(caster);
    r3d_mesh_destroy(body);
    r3d_mesh_destroy(slope);
    r3d_mesh_destroy(cutout);
    r3d_mesh_destroy(glass);
    plat_video_shutdown();
    puts("PASS depth shadows, raised receiver, opacity, resize, union and restoration");
    return 0;
}
