#include "vulkan_device.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    char error[256];
    int missing = argc > 1 && !strcmp(argv[1], "--missing-loader");
    moon_vk_device *device =
        moon_vk_open(missing ? "moon-nonexistent-vulkan-loader" : NULL, error, sizeof error);
    if (missing) {
        int failed = device != NULL || !strstr(error, "Vulkan loader:");
        moon_vk_close(device);
        return failed;
    }
    if (!device) {
        fprintf(stderr, "%s\n", error);
        /* An absent runtime/device is optional; an unexpected initialization
           failure on a Vulkan-capable host must still fail the test. */
        if (strstr(error, "Vulkan loader:") || strstr(error, "No Vulkan device") ||
            strstr(error, "vkCreateInstance (Vulkan result -9)"))
            return 77;
        return 1;
    }
    printf("Vulkan device: %s\n", moon_vk_name(device));
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    const uint32_t patterns[] = {0, UINT32_MAX, 0x4d4f4f4e, 0x01234567};
    int ok = moon_vk_name(device)[0] != 0;
    for (size_t i = 0; i < sizeof patterns / sizeof *patterns && ok; i++) {
        ok = moon_vk_check_transfer(device, patterns[i], patterns[i] ^ negative, error,
                                    sizeof error);
        if (!ok)
            fprintf(stderr, "%s\n", error);
    }
    moon_vk_close(device);
    moon_vk_close(NULL);
    return ok ? 0 : 1;
}
