#ifndef MOON_MOBILE_OPTIONS_H
#define MOON_MOBILE_OPTIONS_H
#include "options_store.h"
#include "mobile_controls.h"
void moon_mobile_options_load(const moon_options *store, moon_mobile_options *out, int *menus);
void moon_mobile_options_save(moon_options *store, const moon_mobile_options *value, int menus);
void moon_mobile_options_preset(moon_options *store, int enhanced);
#endif
