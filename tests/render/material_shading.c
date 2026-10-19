/* P: per-pixel evaluation keeps authored coefficients, pass isolation and opacity. */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "render3d.h"
#include "video_gl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SIZE 128
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "material shading %d: %s\n", __LINE__, #x);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
typedef struct {
    int kind, curved, reflection, unlit, race;
    float specular, shine;
} fixture;

static void normalize(float p[3])
{
    float length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    for (int i = 0; i < 3; i++)
        p[i] /= length;
}

static int sample(const fixture *f, int mode, uint8_t *pixels)
{
    gt4_triangle triangles[2] = {0};
    const float positions[6][3] = {{-2, -2, 3}, {2, -2, 3}, {2, 2, 3},
                                   {-2, -2, 3}, {2, 2, 3},  {-2, 2, 3}};
    for (int i = 0; i < 2; i++) {
        triangles[i].kind = f->kind;
        triangles[i].texture = f->reflection ? -2 : -1;
        triangles[i].state.depth_func = 1;
        triangles[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &triangles[i].v[j];
            memcpy(v->p, positions[i * 3 + j], sizeof v->p);
            v->normal[0] = f->curved ? v->p[0] * .5f : .6f;
            v->normal[1] = f->curved ? v->p[1] * .5f : 0;
            v->normal[2] = f->curved ? -1 : -.8f;
            normalize(v->normal);
            v->shine = f->shine;
            v->alpha = .5f;
            for (int c = 0; c < 3; c++) {
                v->rgb[c] = .8f;
                v->ambient[c] = .5f;
                v->specular[c] = f->specular;
            }
        }
    }
    gt4_model model = {0};
    model.triangles = triangles;
    model.count = 2;
    r3d_mesh *mesh = r3d_mesh_create(&model);
    if (!mesh)
        return 0;
    uint8_t env[256 * 256 * 4];
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            int at = (y * 256 + x) * 4;
            env[at] = x;
            env[at + 1] = y;
            env[at + 2] = 128;
            env[at + 3] = 128;
        }
    tex1_image environment = {0};
    environment.width = environment.height = 256;
    environment.rgba = env;
    r3d_environment_changed();
    r3d_begin(0, 0, 640, 448, &environment);
    if (!r3d_begin_target(SIZE))
        return 0;
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = f->unlit;
    if (f->reflection) {
        view.env_color[0] = .8f;
        view.env_color[1] = -.2f;
    }
    r3d_lights lights = {0};
    lights.count = 1;
    lights.direction[0][2] = -1;
    lights.specular_scale = 1;
    lights.vertex_limit = f->race ? 255.f / 128 : 0;
    for (int c = 0; c < 3; c++) {
        lights.ambient[0][c] = .2f;
        lights.diffuse[0][c] = .25f;
        lights.specular[0][c] = .5f;
    }
    r3d_material_shading(mode);
    r3d_draw_lit(mesh, &view, &lights);
    r3d_end();
    r3d_end_target(pixels);
    r3d_mesh_destroy(mesh);
    return 1;
}

/* Independent CPU calculation for the selected fragment, not shader source reuse. */
static void expected(const fixture *f, float rgb[3])
{
    float ray[3] = {1.f / SIZE, -1.f / SIZE, 1};
    normalize(ray);
    float n[3] = {.6f, 0, -.8f};
    if (f->reflection) {
        float incident[3] = {ray[0], ray[1], -ray[2]};
        float normal[3] = {n[0], n[1], -n[2]};
        float dot = incident[0] * normal[0] + incident[1] * normal[1] + incident[2] * normal[2];
        float reflected[3];
        for (int c = 0; c < 3; c++)
            reflected[c] = incident[c] - 2 * dot * normal[c];
        reflected[2] += 1;
        float length = sqrtf(reflected[0] * reflected[0] + reflected[1] * reflected[1] +
                             reflected[2] * reflected[2]);
        float strength = .8f * (.8f - .2f * fminf(ray[0] * n[0] + ray[2] * n[2], 0));
        rgb[0] = fminf(255, fmaxf(0, (.5f + reflected[0] / (2 * length)) * 256 - .5f)) * strength;
        rgb[1] = fminf(255, fmaxf(0, (.5f - reflected[1] / (2 * length)) * 256 - .5f)) * strength;
        rgb[2] = 128 * strength;
        return;
    }
    float half[3] = {-ray[0], -ray[1], -1 - ray[2]};
    normalize(half);
    float exponent = f->race ? (f->shine < 6 ? 4 : f->shine < 12 ? 8 : 16) : f->shine;
    float spec = f->specular * .5f * powf(fmaxf(0, n[0] * half[0] + n[2] * half[2]), exponent);
    float light = f->kind == 4 ? 0 : .5f * .2f + .8f * .25f * .8f;
    for (int c = 0; c < 3; c++)
        rgb[c] = (light + spec) * 255;
}

int main(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    const int negative = argc > 1 && !strcmp(argv[1], "--negative");
    uint8_t pixels[SIZE * SIZE * 4], original[SIZE * SIZE * 4], restored[SIZE * SIZE * 4];
    const int center = (SIZE / 2 * SIZE + SIZE / 2) * 4;
    int cases = 0, max_error = 0;
    for (int race = 0; race < 2; race++)
        for (int kind = 1; kind <= 4; kind += 3)
            for (int shine = 1; shine <= 16; shine *= 2)
                for (int strength = 0; strength < 3; strength++) {
                    fixture f = {.kind = kind,
                                 .race = race,
                                 .shine = (float)shine,
                                 .specular = strength * .5f};
                    CHECK(sample(&f,1,pixels));
                    float rgb[3];
                    expected(&f, rgb);
                    for (int c = 0; c < 3; c++) {
                        int error = abs((int)pixels[center + c] - (int)floorf(rgb[c] + .5f));
                        if (error > max_error)
                            max_error = error;
                        CHECK(error<=2);
                    }
                    if (abs((int)pixels[center + 3] - 128) > 1)
                        printf("alpha=%u\n", pixels[center + 3]);
                    CHECK(abs((int)pixels[center+3]-128)<=1);
                    cases++;
                }
    fixture f = {.kind = 1, .reflection = 1, .shine = 8, .specular = 1};
    CHECK(sample(&f,1,pixels));
    float rgb[3];
    expected(&f, rgb);
    for (int c = 0; c < 3; c++)
        CHECK(abs((int)pixels[center+c]-(int)floorf(rgb[c]+.5f))<=2);
    memcpy(original, pixels, sizeof pixels);
    f.specular = 0;
    CHECK(sample(&f,1,pixels));
    CHECK(!memcmp(original,pixels,sizeof pixels));
    f = (fixture){.kind = 4, .curved = 1, .shine = 16, .specular = 1, .race = 1};
    CHECK(sample(&f,0,original));
    CHECK(sample(&f,negative?0:1,pixels));
    CHECK(sample(&f,0,restored));
    CHECK(!memcmp(original,restored,sizeof original));
    printf("authored cases=%d max error=%d; curved highlight Original=%u Per-Pixel=%u\n", cases,
           max_error, original[center], pixels[center]);
    CHECK(pixels[center]>original[center]+80);
    f.unlit = 1;
    CHECK(sample(&f,0,original));
    CHECK(sample(&f,1,restored));
    CHECK(!memcmp(original,restored,sizeof original));
    f.kind = 0;
    f.unlit = 0;
    CHECK(sample(&f,0,original));
    CHECK(sample(&f,1,restored));
    CHECK(!memcmp(original,restored,sizeof original));
    r3d_material_shading(0);
    plat_video_shutdown();
    puts(
        "PASS authored coefficients, exponent, reflection, pass separation, unlit, alpha and restoration");
    return 0;
}
