/* Match video_gl.c's full-width clip extension into the 16:9 side margins.
   RaceRankDisplay (003A3B58, 003AFD18) shifts its number left in widescreen. */
#ifndef MOON_DEKO_UI_CLIP_H
#define MOON_DEKO_UI_CLIP_H
#include <cmath>
/* Same full-screen rounding as video_gl.c. FE_TOWARDZERO can leave fitted
   16:9 widths just below their integral framebuffer size. */
static inline void nd_content_viewport(float content_x, float content_width, int &left, int &width)
{
    left = static_cast<int>(std::floor(content_x + .5f));
    width = static_cast<int>(std::floor(content_width + .5f));
}
static inline void nd_clip_horizontal(float x, float w, float view_w, float offset, float scale,
                                      float content_x, float content_width, int &left, int &width)
{
    float rx = std::floor(x), rw = std::floor(w + .5f);
    left = static_cast<int>(std::floor(offset + rx * scale + .5f));
    width = static_cast<int>(std::floor(rw * scale + .5f));
    if (rx <= 0 && rx + rw >= view_w) {
        left = static_cast<int>(std::floor(content_x + .5f));
        width = static_cast<int>(std::floor(content_width + .5f));
    }
    if (left < 0)
        left = 0;
    if (width < 0)
        width = 0;
}
#endif
