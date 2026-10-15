/* P: Switch GPU budget, independent of the original simulation and UI size. */
#ifndef OPENGT4_SWITCH_QUALITY_H
#define OPENGT4_SWITCH_QUALITY_H

static inline void switch_scene_dimensions(int width, int height, int *w, int *h)
{
    int max_w = width >= 1600 && height >= 900 ? 1280 : 960;
    int max_h = width >= 1600 && height >= 900 ? 720 : 540;
    double scale = 1;
    if (width > max_w)
        scale = (double)max_w / width;
    if (height * scale > max_h)
        scale = (double)max_h / height;
    *w = (int)(width * scale + .5);
    *h = (int)(height * scale + .5);
}

#endif
