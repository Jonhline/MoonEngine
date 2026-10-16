/* P: Switch fsdev does not provide POSIX rename-over-existing semantics. */
#ifndef MOON_SWITCH_FILE_H
#define MOON_SWITCH_FILE_H
#include <stddef.h>
int plat_switch_replace_file(const char *source, const char *destination);
int plat_switch_recover_file(const char *destination);
int plat_switch_temp_path(const char *destination, char *out, size_t capacity);
#endif
