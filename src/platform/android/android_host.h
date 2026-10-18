#ifndef MOON_ANDROID_HOST_H
#define MOON_ANDROID_HOST_H
#include "mobile_controls.h"
#include "plat_sdl.h"
#include "ui.h"
void moon_android_init(void);
void moon_android_configure(const moon_mobile_options *options, int running);
int moon_android_event(plat_key_event *event);
void moon_android_overlay(ui_project *ui);
#endif
