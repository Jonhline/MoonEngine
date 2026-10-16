/* The player data folder (saves, replays, photos, options, launcher), SDL's
   pref path for Moon Engine. P: the project was named OpenGT4 before
   16 October 2026; the first time the new folder is empty, the old
   OpenGT4 folder is copied into it (the old one stays as it was). */
#include "moon_pref.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define MARKER "migrated_from_opengt4.txt"

static int empty_dir(const char *path)
{
#ifdef _WIN32
    char pattern[1200];
    snprintf(pattern, sizeof pattern, "%s*", path);
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA(pattern, &d);
    if (h == INVALID_HANDLE_VALUE)
        return 1;
    int empty = 1;
    do
        if (strcmp(d.cFileName, ".") && strcmp(d.cFileName, ".."))
            empty = 0;
    while (empty && FindNextFileA(h, &d));
    FindClose(h);
    return empty;
#else
    DIR *dir = opendir(path);
    if (!dir)
        return 1;
    struct dirent *e;
    int empty = 1;
    while (empty && (e = readdir(dir)))
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            empty = 0;
    closedir(dir);
    return empty;
#endif
}

static int copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb"), *out = in ? fopen(to, "wb") : NULL;
    char buf[65536];
    size_t n;
    int ok = in && out;
    while (ok && (n = fread(buf, 1, sizeof buf, in)) > 0)
        ok = fwrite(buf, 1, n, out) == n;
    if (in)
        fclose(in);
    if (out && fclose(out))
        ok = 0;
    return ok;
}

/* from and to end with a separator */
static int copy_tree(const char *from, const char *to)
{
    int ok = 1;
#ifdef _WIN32
    char pattern[1200];
    snprintf(pattern, sizeof pattern, "%s*", from);
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA(pattern, &d);
    if (h == INVALID_HANDLE_VALUE)
        return 1;
    do {
        if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, ".."))
            continue;
        char a[1200], b[1200];
        snprintf(a, sizeof a, "%s%s", from, d.cFileName);
        snprintf(b, sizeof b, "%s%s", to, d.cFileName);
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            CreateDirectoryA(b, NULL);
            strcat(a, "\\");
            strcat(b, "\\");
            ok &= copy_tree(a, b);
        } else
            ok &= copy_file(a, b);
    } while (FindNextFileA(h, &d));
    FindClose(h);
#else
    DIR *dir = opendir(from);
    if (!dir)
        return 1;
    struct dirent *e;
    while ((e = readdir(dir))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char a[1200], b[1200];
        struct stat st;
        snprintf(a, sizeof a, "%s%s", from, e->d_name);
        snprintf(b, sizeof b, "%s%s", to, e->d_name);
        if (stat(a, &st))
            continue;
        if (S_ISDIR(st.st_mode)) {
            mkdir(b, 0755);
            strcat(a, "/");
            strcat(b, "/");
            ok &= copy_tree(a, b);
        } else
            ok &= copy_file(a, b);
    }
    closedir(dir);
#endif
    return ok;
}

/* the same path with every "MoonEngine" segment as "OpenGT4" */
static void old_path(const char *path, char *out, size_t size)
{
    size_t n = 0;
    for (const char *p = path; *p && n + 8 < size;) {
        if (!strncmp(p, "MoonEngine", 10)) {
            memcpy(out + n, "OpenGT4", 7);
            n += 7;
            p += 10;
        } else
            out[n++] = *p++;
    }
    out[n] = 0;
}

static int exists_dir(const char *path)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return !stat(path, &st) && S_ISDIR(st.st_mode);
#endif
}

char *moon_pref_path(void)
{
    char *path = SDL_GetPrefPath("MoonEngine", "MoonEngine");
    static int checked;
    if (!path || checked)
        return path;
    checked = 1;
    char old[1200], marker[1200];
    old_path(path, old, sizeof old);
    snprintf(marker, sizeof marker, "%s%s", path, MARKER);
    if (strcmp(old, path) && exists_dir(old) && empty_dir(path)) {
        int ok = copy_tree(old, path);
        FILE *f = fopen(marker, "w");
        if (f) {
            fprintf(f, "Copied from %s%s\n", old, ok ? "" : " (some files could not be copied)");
            fclose(f);
        }
        fprintf(stderr, "player data: copied %s to %s%s\n", old, path, ok ? "" : " (incomplete)");
    }
    return path;
}
