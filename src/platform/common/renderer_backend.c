#include "platform/renderer_backend.h"
#include <string.h>

const char *moon_renderer_name(int backend)
{
    switch (backend) {
    case MOON_RENDERER_OPENGL:
        return "opengl";
    case MOON_RENDERER_VULKAN:
        return "vulkan";
    default:
        return "unknown";
    }
}

int moon_renderer_parse(const char *name)
{
    if (name)
        for (int i = 0; i < MOON_RENDERER_COUNT; i++)
            if (!strcmp(name, moon_renderer_name(i)))
                return i;
    return -1;
}

const char *moon_renderer_unavailable(int backend)
{
    if (backend == MOON_RENDERER_OPENGL || backend == MOON_RENDERER_VULKAN)
        return NULL;
    return "Unknown graphics backend. Select OpenGL to play.";
}
