/* Switch-native GPU ownership. No Mesa/EGL objects cross this interface. */
#ifndef OPENGT4_DEKO_DEVICE_H
#define OPENGT4_DEKO_DEVICE_H
#include <deko3d.h>
#include "deko_contract.h"
#include <stddef.h>
#include <stdint.h>
struct nd_buffer {
    DkMemBlock mem;
    uint32_t size;
};
struct nd_image {
    nd_buffer storage;
    DkImage image;
    DkImageView view;
    unsigned slot, width, height;
    bool depth;
};

enum nd_program { ND_UI, ND_MESH, ND_MIX };
int nd_init(unsigned width, unsigned height);
void nd_shutdown();
DkCmdBuf nd_commands();
void nd_begin();
void nd_present();
void nd_sync(); /* Readback/shutdown only, never ordinary frame rendering. */
nd_image *nd_screen();
nd_buffer nd_alloc(uint32_t size, uint32_t flags);
void nd_release(nd_buffer memory);
void *nd_cpu(nd_buffer memory);
DkGpuAddr nd_gpu(nd_buffer memory);
DkGpuAddr nd_stream(const void *bytes, size_t size, unsigned alignment = 256);
nd_image *nd_image_create(unsigned w, unsigned h, bool depth = false, bool present = false);
void nd_image_destroy(nd_image *image);
void nd_image_upload(nd_image *image, const uint8_t *rgba);
bool nd_image_read(nd_image *image, void *bytes);
void nd_copy(nd_image *src, nd_image *dst, unsigned x, unsigned y, unsigned w, unsigned h);
void nd_bind(nd_image *color, nd_image *depth);
void nd_viewport(int x, int y, int w, int h);
void nd_scissor(int x, int y, int w, int h);
void nd_clear(float r, float g, float b, float a, bool color, bool depth);
void nd_pipeline(nd_program program, const nd_uniform &uniforms, nd_buffer vertices, unsigned first,
                 unsigned count, const unsigned images[4], const DkRasterizerState &raster,
                 const DkDepthStencilState &depth, const DkColorState &color,
                 const DkColorWriteState &mask, const DkBlendState &blend, float fixed_alpha,
                 unsigned sampler = 0, DkGpuAddr vertex_address = 0, unsigned probe_flags = 0);
nd_image *nd_image_slot(unsigned slot);
unsigned nd_program_count();
#endif
