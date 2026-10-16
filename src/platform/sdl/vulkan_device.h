#ifndef MOON_VULKAN_DEVICE_H
#define MOON_VULKAN_DEVICE_H

#include <stddef.h>
#include <stdint.h>

/* PC backend foundation. This does not implement the game's drawing API. */
typedef struct moon_vk_device moon_vk_device;
moon_vk_device *moon_vk_open(const char *loader, char *error, size_t capacity);
const char *moon_vk_name(const moon_vk_device *device);
/* Submit a fill, wait on its fence and compare every word after host readback. */
int moon_vk_check_transfer(moon_vk_device *device, uint32_t pattern, uint32_t expected, char *error,
                           size_t capacity);
void moon_vk_close(moon_vk_device *device);

#endif
