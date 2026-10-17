/* Switch-only presentation sizes; no simulation state depends on this choice. */
#ifndef MOON_SWITCH_SCENE_H
#define MOON_SWITCH_SCENE_H
#define MOON_SWITCH_SCENE_KEY "graphics.switch_scene_resolution"
enum { MOON_SWITCH_CLASSIC, MOON_SWITCH_BALANCED, MOON_SWITCH_PERFORMANCE };
static inline void moon_switch_scene_size(int choice, int docked, unsigned *w, unsigned *h)
{
    if (choice == MOON_SWITCH_CLASSIC) {
        *w = 640;
        *h = 448;
    } else if (choice == MOON_SWITCH_PERFORMANCE) {
        *w = 640;
        *h = 360;
    } else {
        *w = docked ? 1280 : 960;
        *h = docked ? 720 : 540;
    }
}
#ifdef __cplusplus
extern "C" {
#endif
void video_switch_scene_resolution(int choice);
#ifdef __cplusplus
}
#endif
#endif
