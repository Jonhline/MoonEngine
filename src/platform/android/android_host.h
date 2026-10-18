#ifndef MOON_ANDROID_HOST_H
#define MOON_ANDROID_HOST_H
#include "mobile_controls.h"
#include "plat_sdl.h"
#include "ui.h"
void moon_android_init(void);
void moon_android_configure(const moon_mobile_options *options, int running);
int moon_android_event(plat_key_event *event);
void moon_android_overlay(ui_project *ui);
/* Record the point the app is at, so the fatal-signal handler can name it in
   the final log line ("last frame <file:line>"). A no-op elsewhere; the string
   must outlive the call (call sites pass a literal). */
void moon_android_mark_frame(const char *where);
/* The two user-state paths that are Android-only, for the startup inventory
   line (main.c). Each returns the path set by moon_android_init, or NULL. */
const char *moon_android_log_path(void);
const char *moon_android_log_mirror(void);
#endif
