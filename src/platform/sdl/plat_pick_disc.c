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
#include <errno.h>
#include <glob.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <SDL.h>
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

/* The folder of the disc already named, so the chooser opens where the image is. */
static void folder_of(const char *path, char *out, size_t cap)
{
    snprintf(out, cap, "%s", path ? path : "");
    char *slash = strrchr(out, '/');
    if (slash && slash != out)
        *slash = 0;
    else if (!strchr(out, '/'))
        out[0] = 0;
}

/* First run has no disc to point at yet, so look for the newest .iso a PC is
   likely to keep one in and open there: the chooser then shows the image
   instead of a home folder with nothing in it. Bounded depth, no recursion. */
static void newest_disc_iso(char *out, size_t cap)
{
    const char *home = getenv("HOME");
    char patterns[6][512];
    int count = 0;
    if (home && *home) {
        snprintf(patterns[count++], 512, "%s/*.iso", home);
        snprintf(patterns[count++], 512, "%s/*/*.iso", home);
        snprintf(patterns[count++], 512, "%s/*/*/*.iso", home);
    }
    snprintf(patterns[count++], 512, "/mnt/*/*.iso");
    snprintf(patterns[count++], 512, "/mnt/*/*/*.iso");
    snprintf(patterns[count++], 512, "/media/*/*/*.iso");
    time_t newest = 0;
    int newest_is_gran_turismo = 0;
    out[0] = 0;
    for (int i = 0; i < count; i++) {
        glob_t found;
        if (glob(patterns[i], 0, NULL, &found) != 0)
            continue;
        for (size_t j = 0; j < found.gl_pathc; j++) {
            struct stat info;
            if (stat(found.gl_pathv[j], &info) != 0 || !S_ISREG(info.st_mode))
                continue;
            if (info.st_size < 1024 * 1024)   /* left-over stubs are not a disc */
                continue;
            /* This engine only runs Gran Turismo, so a name that says so wins over
               a fresher timestamp: copied images keep the disc's old mtime. */
            char lower[512];
            size_t k = 0;
            for (const char *s = found.gl_pathv[j]; *s && k + 1 < sizeof lower; s++)
                lower[k++] = (char)(*s >= 'A' && *s <= 'Z' ? *s - 'A' + 'a' : *s);
            lower[k] = 0;
            int gran_turismo = strstr(lower, "gran turismo") || strstr(lower, "gt4");
            if (out[0] && (newest_is_gran_turismo > gran_turismo ||
                           (newest_is_gran_turismo == gran_turismo && info.st_mtime <= newest)))
                continue;
            newest = info.st_mtime;
            newest_is_gran_turismo = gran_turismo;
            snprintf(out, cap, "%s", found.gl_pathv[j]);
        }
        globfree(&found);
    }
}

/* Read the first line of a picker's stdout. A cancel closes the pipe with
   nothing written, which is the same "no disc chosen" the Windows dialog
   reports, so both paths return 0 and leave *out untouched. The wait keeps the
   engine's own window answering: without this the process looks frozen for as
   long as the chooser is open, because the call blocks its only thread. */
static int read_choice(const char *command, char *out, size_t cap)
{
    FILE *pipe = popen(command, "r");
    if (!pipe)
        return 0;
    size_t used = 0;
    int done = 0, failed = 0;
    while (!done) {
        struct pollfd waiting = {fileno(pipe), POLLIN, 0};
        int ready = poll(&waiting, 1, 50);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            failed = 1;
            break;
        }
        if (ready == 0) {
            SDL_PumpEvents(); /* the window keeps its own messages, not just the chooser's */
            continue;
        }
        char chunk[512];
        size_t got = fread(chunk, 1, sizeof chunk, pipe);
        if (!got)
            break;
        for (size_t i = 0; i < got; i++) {
            if (chunk[i] == '\n') {
                done = 1;
                break;
            }
            if (used + 1 < cap)
                out[used++] = chunk[i];
        }
    }
    out[used] = 0;
    if (pclose(pipe) != 0 || failed) {
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
    /* zenity opens the folder holding the file it is given, so hand it the image
       itself; kdialog and yad take the folder directly. */
    char image[900] = "", start[900] = "";
    const char *hint = getenv("MOONSTUDIO_DISC_HINT");
    struct stat hint_info;
    if (hint && *hint && stat(hint, &hint_info) == 0 && S_ISREG(hint_info.st_mode))
        snprintf(image, sizeof image, "%s", hint);
    if (!image[0])
        newest_disc_iso(image, sizeof image);
    if (image[0])
        folder_of(image, start, sizeof start);
    else if (hint && *hint)
        folder_of(hint, start, sizeof start);
    if (!start[0]) {
        const char *home = getenv("HOME");
        if (home && *home)
            snprintf(start, sizeof start, "%s", home);
    }

    fprintf(stderr, "disc picker: %s (folder %s)\n", image[0] ? image : "(no image found)",
            start[0] ? start : "(none)");
    if (in_path("zenity")) {
        char command[1400];
        snprintf(command, sizeof command,
                 "zenity --file-selection --title='MOON ENGINE - Select game disc' "
                 "--filename='%s' --file-filter='PlayStation 2 disc image | *.iso' "
                 "--file-filter='All files | *' 2>/dev/null",
                 image[0] ? image : start);
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
