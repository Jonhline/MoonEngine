/* Switch-only render work, also compiled by the host GPU comparison fixture. */
#ifndef MOON_SWITCH_RENDER_H
#define MOON_SWITCH_RENDER_H
#include "platform/platform.h"
typedef struct switch_gpu_image switch_gpu_image;
int switch_gpu_warmup(void);
switch_gpu_image *switch_gpu_mix_create(int width, int height,
                                        const unsigned char *const layers[4]);
switch_gpu_image *switch_gpu_env_create(int face_size, int map_size);
void switch_gpu_image_destroy(switch_gpu_image *image);
plat_texture *switch_gpu_image_texture(const switch_gpu_image *image);
void switch_gpu_mix(switch_gpu_image *image, const float weights[4]);
void switch_gpu_env_capture(switch_gpu_image *image, int face);
void switch_gpu_env_remap(switch_gpu_image *image, const float right[3], const float up[3],
                          const float forward[3]);
#endif
