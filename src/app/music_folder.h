/* One discoverable Music folder beside the active options store. */
#ifndef MOON_MUSIC_FOLDER_H
#define MOON_MUSIC_FOLDER_H
#include "save.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif
static int app_music_folder(const char *config, char *folder, size_t size)
{
    if (!config || !*config || !folder || !size) return 0;
    const char *slash = strrchr(config, '/'), *back = strrchr(config, '\\');
    if (!slash || (back && back > slash)) slash = back;
    size_t prefix = slash ? (size_t)(slash - config + 1) : 0;
    if (prefix + sizeof "Music" > size) return 0;
    memcpy(folder, config, prefix);
    memcpy(folder + prefix, "Music", sizeof "Music");
    if (!gt4_save_mkdir(folder)) {
        fprintf(stderr, "music: could not create folder: %s\n", folder);
        return 0;
    }
#ifdef _WIN32
    wchar_t wide[1100];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, folder, -1, wide, 1100)) return 0;
    DWORD attrs = GetFileAttributesW(wide);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return stat(folder, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}
#endif
