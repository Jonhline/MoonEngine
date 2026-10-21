/* P: material-tagged emissive lenses, never nearby body paint. */
#include "render3d.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define N (128 * 128 * 4)
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "lamp emission %d: %s\n", __LINE__, #x);                               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int frame(int enabled, int lens, int kind, int hdr, int brake, unsigned char out[N])
{
    gt4_triangle t[4] = {0};
    const float points[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
    for (int i = 0; i < 4; i++) {
        t[i].texture = -1;
        t[i].kind = kind;
        t[i].night_lens = lens && i < 2;
        if (lens == 2 && i < 2) {
            t[i].night_lens = 0;
            t[i].lamp = 2;
        }
        t[i].state.depth_func = 1;
        t[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &t[i].v[j];
            v->p[0] = (i < 2 ? -.4f : .4f) + points[(i % 2) * 3 + j][0] * .18f;
            v->p[1] = points[(i % 2) * 3 + j][1] * .25f;
            v->p[2] = 3;
            v->normal[2] = -1;
            v->alpha = 1;
            v->rgb[0] = v->rgb[1] = v->rgb[2] = .1f;
        }
    }
    gt4_model model = {0};
    model.triangles = t;
    model.count = 4;
    r3d_mesh *mesh = r3d_mesh_create(&model);
    if (!mesh)
        return 0;
    r3d_car_lamp lamps[2] = {{{-.4f, 0, 3, 8}, {0, 0, 1, .9f}, {3, .045f, .015f, 0}},
                             {{.4f, 0, 3, 8}, {0, 0, 1, .9f}, {3, .045f, .015f, 0}}};
    r3d_car_lamps(enabled);
    r3d_car_lamp_frame(lamps, 2);
    video_hdr_options(hdr, 0);
    plat_video_begin_frame();
    if (hdr)
        video_begin_scene();
    r3d_view v = {0};
    v.rot[0] = v.rot[4] = v.rot[8] = 1;
    v.r[0] = v.u[1] = v.f[2] = 1;
    v.focal = 320;
    v.focal_y = 224;
    v.brake = brake;
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(mesh, &v);
    r3d_end();
    if (hdr)
        video_end_scene();
    int w, h;
    unsigned char *p = plat_sdl_read_pixels(&w, &h);
    int ok = p && w == 128 && h == 128;
    if (ok)
        memcpy(out, p, N);
    free(p);
    plat_video_end_frame();
    r3d_mesh_destroy(mesh);
    return ok;
}
int main(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    unsigned char off[N], on[N], plain[N], restored[N], release[N];
    for (int hdr = 0; hdr <= 1; hdr++)
        for (int kind = 0; kind <= 1; kind++)
            for (int lens = 1; lens <= 2; lens++) {
                CHECK(frame(0,lens,kind,hdr,1,off));
                CHECK(frame(argc>1?0:1,lens,kind,hdr,1,on));
                CHECK(frame(1,0,kind,hdr,1,plain));
                CHECK(frame(0,lens,kind,hdr,1,restored));
                CHECK(!memcmp(off,restored,N));
                int lit = 0;
                for (int y = 50; y < 78; y++)
                    for (int x = 48; x < 62; x++) {
                        int at = (y * 128 + x) * 4;
                        lit += on[at] > off[at] + 20;
                    }
                printf("HDR=%d kind=%d tag=%d emitted_pixels=%d\n", hdr, kind, lens, lit);
                CHECK(lit>10);
                if (hdr) {
                    int halo = 0;
                    for (int y = 52; y < 58; y++)
                        for (int x = 52; x < 60; x++)
                            halo += on[(y * 128 + x) * 4] > off[(y * 128 + x) * 4];
                    printf("bloom halo=%d\n", halo);
                    CHECK(halo>0);
                }
                for (int y = 0; y < 128; y++)
                    for (int x = 64; x < 128; x++) {
                        int at = (y * 128 + x) * 4;
                        CHECK(!memcmp(on+at,plain+at,3));
                    }
                if (lens == 2) {
                    CHECK(frame(1,lens,kind,hdr,0,release));
                    for (int y = 0; y < 128; y++)
                        for (int x = 0; x < 64; x++) {
                            int at = (y * 128 + x) * 4;
                            CHECK(release[at]<20);
                        }
                }
            }
    r3d_car_lamps(0);
    plat_video_shutdown();
    puts(
        "PASS tagged lit/unlit lenses, pressed/released, adjacent paint, HDR and byte-exact Off restoration");
    return 0;
}
