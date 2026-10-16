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
int moon_vk_render_vsync(int enabled);
typedef struct {
    uint64_t submissions, passes, draws, readbacks;
    uint64_t vertex_upload_bytes;
} moon_vk_render_stats;
moon_vk_render_stats moon_vk_render_statistics(void);
#ifdef __cplusplus
}
#endif
#endif
