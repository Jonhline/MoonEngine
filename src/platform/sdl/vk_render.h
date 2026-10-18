#ifndef MOON_VK_RENDER_H
#define MOON_VK_RENDER_H
#include <SDL.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int moon_vk_render_open(SDL_Window *window, int vsync);
void moon_vk_render_close(void);
int moon_vk_render_active(void);
void *moon_vk_render_proc(const char *name);
void moon_vk_render_size(SDL_Window *window, int *width, int *height);
void moon_vk_render_present(void);
/* The window changed size: rebuild the swapchain, keep the surface. */
void moon_vk_render_surface_dirty(void);
/* The window's surface is gone (Android background/picker): rebuild both. */
void moon_vk_render_surface_lost(void);
void moon_vk_render_drain(void);
int moon_vk_render_prepare_geometry(void);
void moon_vk_render_low_latency(int enabled);
int moon_vk_render_vsync(int enabled);
typedef struct {
    uint64_t submissions, passes, draws, readbacks;
    uint64_t vertex_upload_bytes, descriptor_updates, framebuffer_creations;
    uint64_t pipeline_binds, dynamic_sets, vertex_binds, pipeline_warmups;
    uint64_t cpu_prepare_ns, cpu_descriptors_ns, cpu_pipeline_ns, cpu_commands_ns;
    uint64_t frame_rotations, pipeline_cache_bytes;
    uint64_t gpu_ns, gpu_samples, fence_wait_ns, pipeline_ns, pipeline_creations;
} moon_vk_render_stats;
moon_vk_render_stats moon_vk_render_statistics(void);
#ifdef __cplusplus
}
#endif
#endif
