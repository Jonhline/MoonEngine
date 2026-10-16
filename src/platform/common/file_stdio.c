/* plat_file on the C standard library, with 64-bit offsets. */
#include "platform/switch_profile.h"
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef __SWITCH__
#include <sys/types.h>
_Static_assert(sizeof(off_t) >= 8, "Switch disc I/O requires 64-bit offsets");
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct plat_file {
    FILE *fp;
    uint64_t size;
};

#ifdef _WIN32
#define SEEK64(f, o) _fseeki64((f), (long long)(o), SEEK_SET)
#define END64(f) _fseeki64((f), 0, SEEK_END)
#define TELL64(f) ((uint64_t)_ftelli64(f))
#else
#define SEEK64(f, o) fseeko((f), (off_t)(o), SEEK_SET)
#define END64(f) fseeko((f), 0, SEEK_END)
#define TELL64(f) ((uint64_t)ftello(f))
#endif

/* The user's Moon Engine folder: saves, config, the studio rig, and mods. This
   is deliberately in the common layer, not the SDL backend, so tools and probes
   that link only the core can ask where the user's data lives without pulling a
   window library in. It follows the same convention the app's own
   SDL_GetPrefPath calls use, so saves do not move:
     Windows  %APPDATA%\MoonEngine\MoonEngine\
     Linux    $XDG_DATA_HOME/MoonEngine/MoonEngine/ (else ~/.local/share/...)
   Returns the path WITH a trailing separator, ready to be joined with a name. */
int plat_pref_dir(char *out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = 0;
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    if (!base || !*base) return 0;
    snprintf(out, cap, "%s\\MoonEngine\\MoonEngine\\", base);
#else
    const char *base = getenv("XDG_DATA_HOME");
    char home[1024];
    if (!base || !*base) {
        const char *h = getenv("HOME");
        if (!h || !*h) return 0;
        snprintf(home, sizeof home, "%s/.local/share", h);
        base = home;
    }
    snprintf(out, cap, "%s/MoonEngine/MoonEngine/", base);
#endif
    return out[0] ? 1 : 0;
}

plat_file *plat_file_open(const char *path)
{
    plat_file *f = calloc(1, sizeof *f);
    if (!f)
        return NULL;
#ifdef _WIN32
    /* P: file selectors return UTF-8; use Windows wide paths for ISO names. */
    wchar_t wide[32768];
    f->fp = path && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, 32768)
                ? _wfopen(wide, L"rb")
                : NULL;
#else
    f->fp = path ? fopen(path, "rb") : NULL;
#endif
    if (!f->fp) {
        free(f);
        return NULL;
    }
    END64(f->fp);
    f->size = TELL64(f->fp);
    return f;
}

uint64_t plat_file_size(plat_file *f)
{
    return f->size;
}

size_t plat_file_read_at(plat_file *f, uint64_t offset, void *dst, size_t n)
{
#ifdef __SWITCH__
    SWP_PUSH(SWP_DISC);
    if (offset > f->size || SEEK64(f->fp, offset)) {
        SWP_POP();
        return 0;
    }
    size_t read = fread(dst, 1, n, f->fp);
    SWP_POP();
    return read;
#else
    if (offset > f->size || SEEK64(f->fp, offset))
        return 0;
    return fread(dst, 1, n, f->fp);
#endif
}

void plat_file_close(plat_file *f)
{
    if (!f)
        return;
    fclose(f->fp);
    free(f);
}

#ifndef _WIN32
#include <sys/statvfs.h>
#endif

int plat_disk_free(const char *path, uint64_t *bytes)
{
#ifdef _WIN32
    ULARGE_INTEGER avail;
    if (!path || !bytes || !GetDiskFreeSpaceExA(path, &avail, NULL, NULL))
        return 0;
    *bytes = avail.QuadPart;
#else
    struct statvfs s;
    if (!path || !bytes || statvfs(path, &s))
        return 0;
    *bytes = (uint64_t)s.f_bavail * s.f_frsize;
#endif
    return 1;
}
