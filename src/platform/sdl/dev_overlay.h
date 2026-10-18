#ifndef MOON_DEV_OVERLAY_H
#define MOON_DEV_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* G6.40 debug overlay. One compact readout over whatever is on screen.
   Off at start every run; toggled by F3 (desktop, edge-triggered) or the
   volume-up + volume-down chord (Android). Draws through the platform 2D API
   only; never touches the renderers or the physics step. */
void moon_debug_overlay_init(void);
void moon_debug_overlay_toggle(void);
int moon_debug_overlay_on(void);
/* Call once per frame, after all of the frame's own drawing. No-op when off. */
void moon_debug_overlay_draw(void);

#ifdef __cplusplus
}
#endif

#endif
