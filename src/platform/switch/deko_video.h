#ifndef OPENGT4_DEKO_VIDEO_H
#define OPENGT4_DEKO_VIDEO_H
#include "deko_device.h"
extern "C" {
#include "platform/platform.h"
#include "plat_sdl.h"
#include "video_gl.h"
}
struct plat_texture {
    nd_image *image;
    bool screen_capture;
    bool gpu_capture;
};
struct nv_state {
    nd_image *color, *depth;
    int viewport[4], clip[4];
};
nv_state nv_get();
void nv_set(const nv_state &state);
void nv_target(nd_image *color, nd_image *depth);
void nv_view(int x, int y, int w, int h);
unsigned nv_white();
unsigned nv_black();
void nv_draw_mix(nd_image *target, nd_image *const layers[4], const float weights[4]);
#endif
