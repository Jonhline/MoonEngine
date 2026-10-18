/* The platform's native disc picker. Android supplies its own in android_host.c. */
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

int plat_pick_disc(char *out, size_t cap)
{
    wchar_t wfile[32768] = {0};
    OPENFILENAMEW o;
    memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = NULL;
    o.lpstrFilter = L"PlayStation 2 disc image (*.iso)\0*.iso\0All files\0*.*\0";
    o.lpstrFile = wfile;
    o.nMaxFile = 32768;
    o.lpstrTitle = L"MOON ENGINE - Select game disc";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&o))
        return 0;
    return WideCharToMultiByte(CP_UTF8, 0, wfile, -1, out, (int)cap, NULL, NULL) > 0;
}

#else

int plat_pick_disc(char *out, size_t cap)
{
    /* No OS picker on this backend: the dev path takes the disc from the environment. */
    const char *env = getenv("MOONSTUDIO_ISO");
    if (env && *env) {
        snprintf(out, cap, "%s", env);
        return 1;
    }
    return 0;
}

#endif
