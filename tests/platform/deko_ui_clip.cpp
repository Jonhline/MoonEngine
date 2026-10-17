#include "../../src/platform/switch/deko_ui_clip.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
int main(int argc, char **argv)
{
    int negative = argc > 1 && !std::strcmp(argv[1], "--negative");
    int viewport_negative = argc > 1 && !std::strcmp(argv[1], "--viewport-negative");
    for (int framebuffer : {960, 1280, 1920}) {
        // Fitted bounds one representable value below the exact integer.
        float fitted = std::nextafter(static_cast<float>(framebuffer), 0.f);
        int left, width;
        nd_content_viewport(0, fitted, left, width);
        if (viewport_negative) width = static_cast<int>(fitted);
        if (left != 0 || width != framebuffer) return 1;
    }
    for (float scale : {1.f, 1.5f, 2.25f}) {
        // Original HUD rank centre = 56-96; width=42, left=centre-21.
        float offset = 106.6666667f * scale;
        int left, width;
        nd_clip_horizontal(0, 640, 640, offset, scale, 0, 853.3333333f * scale, left, width);
        if (negative) {
            left = static_cast<int>(offset + .5f);
            width = static_cast<int>(640 * scale + .5f);
        }
        float digit_left = offset + (56 - 96 - 21) * scale;
        float digit_right = digit_left + 42 * scale;
        if (left != 0 || digit_left < left || digit_right > left + width)
            return 1;
        // Local clips must stay local, including inside a UI panel.
        nd_clip_horizontal(100, 40, 640, offset, scale, 0, 853.3333333f * scale, left, width);
        if (left != static_cast<int>(offset + 100 * scale + .5f) ||
            width != static_cast<int>(40 * scale + .5f))
            return 1;
        nd_clip_horizontal(0, 640, 640, 180, scale, 20, 1280, left, width);
        if (left != 20 || width != 1280)
            return 1;
    }
    std::puts("Widescreen rank, local and panel clip coverage PASS");
    return 0;
}
