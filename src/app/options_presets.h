#ifndef MOON_OPTIONS_PRESETS_H
#define MOON_OPTIONS_PRESETS_H
#include "options_store.h"
/* Portable presentation/input preferences, never disc/save/device paths.
   Parsing and merging are transactional; callers decide when to persist. */
int moon_preset_write(const moon_options *values, const char *path, const char *name,
                      char *error, unsigned error_size);
int moon_preset_read(moon_options *values, const char *path, char *error, unsigned error_size);
int moon_preset_merge(moon_options *destination, const moon_options *preset);
#endif
