/* The platform's native disc picker. Android supplies its own in android_host.c. */
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#else
#include <unistd.h>
#endif

#if defined(_WIN32)

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

/* The desktop's own file chooser, the Linux answer to GetOpenFileNameW.
   There is no single picker on Linux: GNOME/GTK ships zenity, KDE ships
   kdialog, and some minimal desktops ship yad. All three print the chosen
   path on stdout and return non-zero when the user cancels, so the first one
   that exists is used. Without any of them the wizard still accepts a drag
   and drop or MOONSTUDIO_ISO, and the caller's message says so. */
static int in_path(const char *name)
{
    const char *path = getenv("PATH");
    if (!path || !name || !*name)
        return 0;
    size_t length = strlen(name);
    for (const char *p = path; *p;) {
        const char *end = strchr(p, ':');
        size_t part = end ? (size_t)(end - p) : strlen(p);
        char candidate[1024];
        int written = part ? snprintf(candidate, sizeof candidate, "%.*s/%s", (int)part, p, name)
                           : snprintf(candidate, sizeof candidate, "./%s", name);
        if (written > 0 && (size_t)written < sizeof candidate && access(candidate, X_OK) == 0)
            return 1;
        if (!end)
            break;
        p = end + 1;
    }
    return 0;
}

/* Read the first line of a picker's stdout. A cancel closes the pipe with
   nothing written, which is the same "no disc chosen" the Windows dialog
   reports, so both paths return 0 and leave *out untouched. */
static int read_choice(const char *command, char *out, size_t cap)
{
    FILE *pipe = popen(command, "r");
    if (!pipe)
        return 0;
    size_t used = 0;
    int c;
    while ((c = fgetc(pipe)) != EOF && used + 1 < cap) {
        if (c == '\n')
            break;
        out[used++] = (char)c;
    }
    out[used] = 0;
    if (pclose(pipe) != 0) {
        out[0] = 0;
        return 0;
    }
    return used > 0;
}

int plat_pick_disc(char *out, size_t cap)
{
    if (!out || cap < 2)
        return 0;
    out[0] = 0;
    const char *home = getenv("HOME");
    char start[900] = "";
    if (home && *home)
        snprintf(start, sizeof start, "%s", home);

    if (in_path("zenity")) {
        char command[1400];
        snprintf(command, sizeof command,
                 "zenity --file-selection --title='MOON ENGINE - Select game disc' "
                 "--filename='%s/' --file-filter='PlayStation 2 disc image | *.iso' "
                 "--file-filter='All files | *' 2>/dev/null",
                 start);
        if (read_choice(command, out, cap)) {
            fprintf(stderr, "disc picker: chose %s\n", out);
            return 1;
        }
    }
    if (in_path("kdialog")) {
        char command[1400];
        snprintf(command, sizeof command,
                 "kdialog --getopenfilename '%s' '*.iso|PlayStation 2 disc image' "
                 "--title 'MOON ENGINE - Select game disc' 2>/dev/null",
                 start);
        if (read_choice(command, out, cap))
            return 1;
    }
    if (in_path("yad")) {
        char command[1400];
        snprintf(command, sizeof command,
                 "yad --file --title='MOON ENGINE - Select game disc' "
                 "--file-filter='*.iso' 2>/dev/null");
        if (read_choice(command, out, cap))
            return 1;
    }
    /* Nothing installed: the dev path, then let the wizard explain. */
    const char *env = getenv("MOONSTUDIO_ISO");
    if (env && *env) {
        snprintf(out, cap, "%s", env);
        return 1;
    }
    return 0;
}

#endif
