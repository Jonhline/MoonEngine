/* P: generated reflection resolution resolves spatial detail without changing the scene. */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "render3d.h"
#include "switch_render.h"
#include "video_gl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "reflection quality %d: %s\n", __LINE__, #x);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static r3d_mesh *pattern(void)
{
    uint8_t pixels[512 * 4];
    for (int x = 0; x < 512; x++) {
        pixels[x * 4] = pixels[x * 4 + 1] = pixels[x * 4 + 2] = (x / 2) % 2 ? 255 : 0;
        pixels[x * 4 + 3] = 128;
    }
    tex1_image image = {0};
    image.width = image.tex_w = 512;
    image.height = image.tex_h = 1;
    image.rgba = pixels;
    gt4_triangle tris[2] = {0};
    const float points[6][3] = {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1},
                                {-1, -1, 1}, {1, 1, 1},  {-1, 1, 1}};
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &tris[i].v[j];
            memcpy(v->p, points[i * 3 + j], sizeof v->p);
            v->uv[0] = (v->p[0] + 1) * .5f;
            v->uv[1] = .5f;
            v->alpha = 1;
            v->rgb[0] = v->rgb[1] = v->rgb[2] = 1;
            tris[i].state.depth_func = 1;
        }
    gt4_model m = {0};
    m.triangles = tris;
    m.count = 2;
    m.textures = &image;
    m.ntextures = 1;
    return r3d_mesh_create(&m);
}
static uint8_t *capture(r3d_mesh *mesh, int mode, int *size)
{
    r3d_reflection_quality(mode);
    *size = r3d_reflection_size();
    switch_gpu_image *env = switch_gpu_env_create(*size, *size);
    if (!env)
        return NULL;
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    r3d_begin(0, 0, 640, 448, NULL);
    for (int face = 0; face < 6; face++) {
        if (!r3d_begin_target(*size))
            return NULL;
        r3d_draw(mesh, &view);
        r3d_end();
        switch_gpu_env_capture(env, face);
    }
    switch_gpu_env_remap(env, view.r, view.u, view.f);
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           video_texture_id(switch_gpu_image_texture(env)), 0);
    uint8_t *pixels = malloc((size_t)*size * *size * 4);
    if (pixels)
        glReadPixels(0, 0, *size, *size, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, video_draw_framebuffer());
    glDeleteFramebuffers(1, &fbo);
    switch_gpu_image_destroy(env);
    return pixels;
}
static float detail(const uint8_t *pixels, int size)
{
    double sum = 0, squared = 0;
    int count = 0;
    for (int y = size * 7 / 16; y < size * 9 / 16; y++)
        for (int x = size * 7 / 16; x < size * 9 / 16; x++) {
            float value = pixels[(y * size + x) * 4];
            sum += value;
            squared += value * value;
            count++;
        }
    return (float)sqrt(fmax(0, squared / count - (sum / count) * (sum / count)));
}
int main(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    r3d_mesh *mesh = pattern();
    CHECK(mesh);
    int n[4];
    uint8_t *images[4];
    float contrast[3];
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    for (int mode = 0; mode < 3; mode++) {
        images[mode] = capture(mesh, negative && mode == 2 ? 0 : mode, &n[mode]);
        CHECK(images[mode]);
        contrast[mode] = detail(images[mode], n[mode]);
        printf("reflection size=%d spatial standard deviation=%.3f\n", n[mode], contrast[mode]);
        if (!negative)
            CHECK(n[mode]==128<<mode);
    }
    CHECK(contrast[1]>contrast[0]+10&&contrast[2]>contrast[0]+10);
    images[3] = capture(mesh, 0, &n[3]);
    CHECK(images[3]&&n[3]==128);
    CHECK(!memcmp(images[0],images[3],128*128*4));
    for (int i = 0; i < 4; i++)
        free(images[i]);
    r3d_reflection_quality(99);
    CHECK(r3d_reflection_size()==128);
    r3d_mesh_destroy(mesh);
    plat_video_shutdown();
    puts("PASS generated detail, all resolutions, invalid mode and Original restoration");
    return 0;
}
