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
/* The scene target is about to be cleared: the next pass may discard its old contents. */
void moon_vk_scene_begin(int first);
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
/* MOON_VK_PROFILE_PASSES: GPU time per pass since the last reset, to stderr. */
void moon_vk_render_pass_report(void);
void moon_vk_render_pass_reset(void);
void moon_vk_gpu_mark(const char *label);
int moon_vk_render_vsync(int enabled);
typedef struct {
    uint64_t submissions, passes, draws, readbacks;
    uint64_t vertex_upload_bytes, uniform_upload_bytes, streamed_draws, resident_draws;
    uint64_t descriptor_updates, framebuffer_creations;
    uint64_t pipeline_binds, dynamic_sets, vertex_binds, pipeline_warmups;
    uint64_t cpu_prepare_ns, cpu_descriptors_ns, cpu_pipeline_ns, cpu_commands_ns;
    uint64_t frame_rotations, pipeline_cache_bytes;
    uint64_t gpu_ns, gpu_samples, fence_wait_ns, pipeline_ns, pipeline_creations;
    uint64_t pipeline_lookups, pipeline_misses, descriptor_builds;
    uint64_t barriers, update_buffer, queue_waits, mem_flushes;
    uint64_t last_submit_ns, last_present_ns;
    uint64_t draw_track, draw_car, draw_shadow, draw_ui, draw_other;
    uint64_t cpu_draw_frame_ns;
    uint64_t vk_bind_pipeline, vk_bind_descriptors, vk_bind_vertices, vk_set_viewport,
             vk_set_scissor, vk_draw, vk_barrier, vk_pipeline_barrier, vk_push_constants;
    uint64_t batch_draws;
    uint64_t program_draws[64];
    uint64_t program_source[64][5];
} moon_vk_render_stats;
moon_vk_render_stats moon_vk_render_statistics(void);
#ifdef __cplusplus
}
#endif
#endif
