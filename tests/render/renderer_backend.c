#include "platform/renderer_backend.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int expected = MOON_RENDERER_VULKAN;
    if (argc > 1 && !strcmp(argv[1], "--negative"))
        expected = MOON_RENDERER_OPENGL;
    int ok = moon_renderer_parse("vulkan") == expected &&
             moon_renderer_parse("opengl") == MOON_RENDERER_OPENGL &&
             moon_renderer_parse(NULL) == -1 && moon_renderer_parse("") == -1 &&
             moon_renderer_parse("invalid") == -1 &&
             !moon_renderer_unavailable(MOON_RENDERER_OPENGL) &&
             !moon_renderer_unavailable(MOON_RENDERER_VULKAN) && moon_renderer_unavailable(-1) &&
             !strcmp(moon_renderer_name(MOON_RENDERER_COUNT), "unknown");
    if (!ok)
        fprintf(stderr, "Renderer selection/readiness mismatch\n");
    return ok ? 0 : 1;
}
