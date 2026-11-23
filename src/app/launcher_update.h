#ifndef MOON_LAUNCHER_UPDATE_H
#define MOON_LAUNCHER_UPDATE_H
#include <stdint.h>
/* MOON_VERSION from the CMake cache (tools/configure_update.py -> generated/) */
#include "launcher_release.h"
#ifndef MOON_VERSION
#define MOON_VERSION "0.4.0"
#endif
typedef struct {
    char version[32], setup[160], sha256[65], notes[2048];
    uint64_t size;
} launcher_update_manifest;
int launcher_update_newer(const char *remote, const char *local);
int launcher_update_parse(const char *json, launcher_update_manifest *manifest);
#ifdef _WIN32
typedef struct launcher_update_job launcher_update_job;
launcher_update_job *launcher_update_start(const char *url, const char *token,
                                           const launcher_update_manifest *download);
/* Results are read only after completion. Errors stay silent during check. */
int launcher_update_poll(launcher_update_job *job, launcher_update_manifest *manifest,
                        const wchar_t **setup);
void launcher_update_release(launcher_update_job *job);
int launcher_update_install(launcher_update_job *job);
int launcher_update_verify(const wchar_t *path, const launcher_update_manifest *manifest);
#endif
#endif
