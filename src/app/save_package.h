#ifndef MOON_SAVE_PACKAGE_H
#define MOON_SAVE_PACKAGE_H
#include "save.h"
#include <stdint.h>
typedef struct {
    unsigned files;
    uint64_t bytes;
    char error[256];
} moon_save_package_report;
/* One selected profile plus the complete shared media/import library.
   Host/disc installation directories and other player slots are excluded. */
int moon_save_package_is_file(const char *path);
int moon_save_package_export(int slot, const char *path, void (*pump)(void *), void *user,
                             moon_save_package_report *report);
int moon_save_package_import(int slot, const char *path, gt4_save_writer writer,
                             void (*pump)(void *), void *user, moon_save_package_report *report);
#endif
