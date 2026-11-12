/* P: a blocker behind the viewing camera must still occlude a spotlight.
   The blocker is never drawn in the receiver pass. Both APIs run this fixture. */
#include "render3d.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "miniz.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BYTES (128 * 128 * 4)
static int failed;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "lamp occlusion %d: %s\n", __LINE__, #x);                              \
            failed = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
static r3d_mesh *quad(float x, float z, float radius, int glass, int cutout)
{
    gt4_triangle t[2] = {0};
    float points[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
    for (int i = 0; i < 2; i++) {
        t[i].texture = -1;
        t[i].kind = 1;
        t[i].state.depth_func = 1;
        t[i].state.depth_write = 1;
        t[i].blend_enabled = glass;
        t[i].state.alpha_test = cutout;
        t[i].state.alpha_func = 6;
        t[i].state.alpha_ref = 64;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &t[i].v[j];
            v->p[0] = x + points[i * 3 + j][0] * radius;
            v->p[1] = points[i * 3 + j][1] * radius;
            v->p[2] = z;
            v->normal[2] = -1;
            v->rgb[0] = v->rgb[1] = v->rgb[2] = .1f;
            v->alpha = cutout ? 0 : 1;
        }
    }
    gt4_model m = {0};
    m.triangles = t;
    m.count = 2;
    return r3d_mesh_create(&m);
}
static int frame(r3d_mesh *receiver, r3d_mesh *blocker, int enabled, int shadow, int moved,
                 int slot, int hdr, unsigned char out[BYTES])
{
    r3d_car_lamp lamps[R3D_CAR_LAMPS] = {0};
    for (int i = 0; i <= slot; i++) {
        lamps[i] = (r3d_car_lamp){{-4, 0, 0, 60}, {.5547002f, 0, .8320503f, .9f}, {0, 0, 0, 0}};
    }
    lamps[slot].color[0] = 6;
    lamps[slot].color[1] = 5.4f;
    lamps[slot].color[2] = 4.5f;
    r3d_car_lamps(enabled);
    r3d_car_lamp_frame(lamps, slot + 1);
    video_hdr_options(hdr, 0);
    plat_video_begin_frame();
    if (hdr)
        video_begin_scene();
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.cam[2] = 4;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    view.color_scale = 1;
    r3d_lamp_shadow *atlas = NULL;
    if (shadow && r3d_lamp_shadow_begin(&atlas)) {
        if (!r3d_lamp_shadow_light(atlas, slot)) {
            r3d_lamp_shadow_destroy(atlas);
            return 0;
        }
        r3d_view object = view;
        object.pos[0] = moved ? 10 : 0;
        r3d_lamp_shadow_mesh(atlas, blocker, &object);
        r3d_lamp_shadow_mesh(atlas, receiver, &view);
        r3d_lamp_shadow_end_light(atlas);
        r3d_lamp_shadow_bind(atlas);
    }
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(receiver, &view);
    r3d_end();
    if (hdr)
        video_end_scene();
    int w, h;
    uint8_t *p = plat_sdl_read_pixels(&w, &h);
    int ok = p && w == 128 && h == 128;
    if (ok)
        memcpy(out, p, BYTES);
    free(p);
    plat_video_end_frame();
    r3d_lamp_shadow_destroy(atlas);
    return ok;
}
static void save(const unsigned char *p, const char *name)
{
    const char *prefix = getenv("LAMP_SHADOW_CAPTURE");
    if (!prefix)
        return;
    char path[1024];
    snprintf(path, sizeof path, "%s-%s.png", prefix, name);
    size_t n;
    void *png = tdefl_write_image_to_png_file_in_memory(p, 128, 128, 4, &n);
    FILE *f = png ? fopen(path, "wb") : NULL;
    if (f) {
        fwrite(png, 1, n, f);
        fclose(f);
    }
    mz_free(png);
}
int main(int argc, char **argv)
{
    int started = 0;
    r3d_mesh *receiver = NULL, *blocker = NULL, *glass = NULL, *cutout = NULL;
    unsigned char off[BYTES], clear[BYTES], blocked[BYTES], moved[BYTES], transparent[BYTES],
        last[BYTES], restored[BYTES];
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    started = 1;
    SDL_HideWindow(plat_sdl_window());
    receiver = quad(0, 6, 1, 0, 0);
    blocker = quad(-2, 3, .6f, 0, 0);
    glass = quad(-2, 3, .6f, 1, 0);
    cutout = quad(-2, 3, .6f, 0, 1);
    CHECK(receiver && blocker && glass && cutout);
    int center = (64 * 128 + 64) * 4;
    for (int hdr = 0; hdr <= 1; hdr++) {
        CHECK(frame(receiver,blocker,0,0,0,0,hdr,off));
        CHECK(frame(receiver,blocker,1,0,0,0,hdr,clear));
        CHECK(frame(receiver,blocker,1,argc>1?0:1,0,0,hdr,blocked));
        CHECK(frame(receiver,blocker,1,1,1,0,hdr,moved));
        CHECK(frame(receiver,blocker,1,1,0,23,hdr,last));
        CHECK(frame(receiver,glass,1,1,0,0,hdr,transparent));
        printf("HDR=%d off=%d lit=%d blocked=%d moved=%d last=%d glass=%d\n", hdr, off[center],
               clear[center], blocked[center], moved[center], last[center], transparent[center]);
        CHECK(clear[center]>off[center]+10);
        CHECK(abs(blocked[center]-off[center])<=1 && abs(last[center]-off[center])<=1);
        CHECK(!memcmp(clear,moved,BYTES) && !memcmp(clear,transparent,BYTES));
        CHECK(frame(receiver,cutout,1,1,0,0,hdr,transparent));
        CHECK(!memcmp(clear,transparent,BYTES));
        CHECK(frame(receiver,blocker,0,0,0,0,hdr,restored));
        CHECK(!memcmp(off,restored,BYTES));
        if (!hdr) {
            save(off, "off");
            save(clear, "clear");
            save(blocked, "blocked");
        }
    }
cleanup:
    if (started) {
        r3d_mesh_destroy(receiver);
        r3d_mesh_destroy(blocker);
        r3d_mesh_destroy(glass);
        r3d_mesh_destroy(cutout);
        plat_video_shutdown();
    }
    if (!failed)
        puts(
            "PASS off-camera blocker, moved blocker, glass/cutout, atlas slot 23, HDR and Classic restore");
    return failed;
}
