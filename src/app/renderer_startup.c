#include "renderer_startup.h"
#include "vulkan_device.h"
#include <stdio.h>
#include <string.h>

int moon_renderer_probe_vulkan(char *text, unsigned long capacity)
{
    char error[256] = "";
    moon_vk_device *device = moon_vk_open(NULL, error, sizeof error);
    if (!device) {
        snprintf(text, capacity, "%s", error[0] ? error : "no Vulkan device");
        return 0;
    }
    snprintf(text, capacity, "%s", moon_vk_name(device));
    moon_vk_close(device);
    return 1;
}
int moon_renderer_diagnostic(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "--vulkan-check"))
        return -1;
    char error[256];
    moon_vk_device *device = moon_vk_open(NULL, error, sizeof error);
    if (!device) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    int ok = moon_vk_check_transfer(device, 0x4d4f4f4e, 0x4d4f4f4e, error, sizeof error);
    fprintf(stderr, "Moon Engine Vulkan: %s; transfer/readback %s\n", moon_vk_name(device),
            ok ? "passed" : error);
    moon_vk_close(device);
    return ok ? 0 : 1;
}
