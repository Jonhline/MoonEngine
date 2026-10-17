#ifndef MOON_DIRENT_H
#define MOON_DIRENT_H
/* <dirent.h> where the platform has it; a minimal one over FindFirstFile on
   Windows (opendir/readdir/closedir, d_name and d_type), plus strcasecmp. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_REG 8
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
struct dirent {
    unsigned char d_type;
    char d_name[MAX_PATH];
};
typedef struct {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    int first;
    struct dirent entry;
} DIR;
static DIR *opendir(const char *path)
{
    char pattern[MAX_PATH + 4];
    if (!path || snprintf(pattern, sizeof pattern, "%s\\*", path) >= (int)sizeof pattern)
        return NULL;
    DIR *d = calloc(1, sizeof *d);
    if (!d)
        return NULL;
    d->handle = FindFirstFileA(pattern, &d->data);
    if (d->handle == INVALID_HANDLE_VALUE) {
        free(d);
        return NULL;
    }
    d->first = 1;
    return d;
}
static struct dirent *readdir(DIR *d)
{
    if (!d)
        return NULL;
    if (!d->first && !FindNextFileA(d->handle, &d->data))
        return NULL;
    d->first = 0;
    snprintf(d->entry.d_name, sizeof d->entry.d_name, "%s", d->data.cFileName);
    d->entry.d_type = (d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? DT_DIR : DT_REG;
    return &d->entry;
}
static int closedir(DIR *d)
{
    if (!d)
        return -1;
    FindClose(d->handle);
    free(d);
    return 0;
}
#else
#include <dirent.h>
#include <strings.h>
#endif
#endif
