/* Original after_fire.mdl endpoint streams, both renderer APIs. */
#include "render3d.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "vol.h"
#include "miniz.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "backfire tween %d: %s\n", __LINE__, #x);                              \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static unsigned char *frame(r3d_mesh *mesh, int pass, float fraction, int tint, const char *path)
{
    plat_video_begin_frame();
    r3d_view v = {0};
    v.rot[0] = 1;
    if (pass) {
        v.rot[5] = -1;
        v.rot[7] = 1;
    } else
        v.rot[4] = v.rot[8] = 1;
    v.pos[2] = 3;
    v.r[0] = v.u[1] = v.f[2] = 1;
    v.focal = 320;
    v.focal_y = 224;
    v.unlit = 1;
    v.color_scale = 1;
    v.grasp = fraction;
    v.authored_color = 1;
    v.color_mul[0] = tint ? .7f : 1;
    v.color_mul[1] = tint ? .52f : 1;
    v.color_mul[2] = tint ? .49f : 1;
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(mesh, &v);
    r3d_end();
    int w, h;
    unsigned char *pixels = plat_sdl_read_pixels(&w, &h);
    if (pixels && path) {
        size_t n;
        void *png = tdefl_write_image_to_png_file_in_memory(pixels, w, h, 4, &n);
        FILE *f = png ? fopen(path, "wb") : NULL;
        if (f) {
            fwrite(png, 1, n, f);
            fclose(f);
        }
        mz_free(png);
    }
    plat_video_end_frame();
    return pixels;
}
int main(int argc, char **argv)
{
    CHECK(argc>=3);
    const char *err = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    CHECK(vol);
    size_t n;
    unsigned char *data = gt4_vol_read(vol, "/race/after_fire.mdl", &n);
    CHECK(data);
    CHECK(!plat_video_init(&(plat_video_desc){320,224,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    for (int pass = 0; pass < 2; pass++)
        for (int branch = 0; branch < 6; branch++) {
            gt4_model *model = gt4_model_decode_afterfire(data, n, pass, branch);
            CHECK(model && model->morph && model->morph_color && model->count>0);
            int different = 0;
            float colour_delta = 0, visible_alpha = 0;
            for (int i = 0; i < model->count; i++)
                for (int k = 0; k < 3; k++) {
                    const gt4_vertex *v = &model->triangles[i].v[k];
                    const gt4_morph_vertex *m = &model->morph[i].v[k];
                    colour_delta += fabsf(v->alpha - m->alpha);
                    visible_alpha = fmaxf(visible_alpha, (v->alpha + m->alpha) * .5f);
                    CHECK(v->alpha>=0 && v->alpha<=1 && m->alpha>=0 && m->alpha<=1);
                    for (int c = 0; c < 3; c++) {
                        CHECK(v->rgb[c]>=0 && v->rgb[c]<=1 && m->rgb[c]>=0 && m->rgb[c]<=1);
                        different |= v->p[c] != m->p[c];
                    }
                }
            CHECK(pass || different);
            float model_alpha_ref = model->triangles[0].state.alpha_ref / 128.f;
            r3d_mesh *mesh = r3d_mesh_create(model);
            CHECK(mesh);
            gt4_model_free(model);
            unsigned char *off = frame(mesh, pass, 0, 1, NULL),
                          *half = frame(mesh, pass, argc > 3 ? 0 : .5f, 1, NULL);
            CHECK(off && half);
            unsigned changed = 0;
            for (unsigned i = 0; i < 320 * 224 * 4; i++)
                changed += off[i] != half[i];
            CHECK(colour_delta==0 || visible_alpha <= model_alpha_ref || changed>20);
            if (branch == 0) {
                char name[1024];
                snprintf(name, sizeof name, "%s-%s.png", argv[2], pass ? "ground" : "flame");
                unsigned char *full = frame(mesh, pass, 1, 1, name),
                              *white = frame(mesh, pass, 1, 0, NULL);
                CHECK(full && white && memcmp(full,white,320*224*4));
                free(full);
                free(white);
            }
            free(off);
            free(half);
            r3d_mesh_destroy(mesh);
        }
    plat_video_shutdown();
    free(data);
    gt4_vol_close(vol);
    puts(
        "backfire: twelve authored colour/geometry endpoints render, zero fraction is dark and tint changes pixels");
    return 0;
}
