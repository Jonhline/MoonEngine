/* P: real prints of the game's own menus for the Options column, in place of drawn pictures
   (Jonhline: "usa prints, nao arte de ia"; generated on each player's PC on first use, in
   the background, and kept). The launcher runs its own executable, hidden and mute, once a
   picture: the menu, the option's value in a scratch options file, scripted input, then
   --shot. One at a time on a thread, started the first time OPTIONS opens; the row on screen
   goes first. The PNGs stay next to launcher.ini (launcher-prints/), so later runs only read
   them. Nothing is shipped: every picture comes from the player's own disc. A row whose
   difference a still picture cannot show has no recipe and no picture. */
#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
#define LAUNCHER_PRINTS 1
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_WINDOWS_UTF8
#include "../../third_party/stb/stb_image.h"
#include <ctype.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#endif
#define PRINTS_VERSION "1" /* a new number takes every print again */
#define PRINTS_WIDTH 640   /* the widest a kept picture is */
typedef struct {
    const char *label;     /* the setting's row */
    const char *names[2];  /* the two sides */
    int values[2];         /* the row's value each side shows */
    const char *config[2]; /* options.ini lines for each side */
    const char *menu, *root, *inputs, *interval, *delay;
    float crop[4]; /* u0 v0 u1 v1 of the print, from its top left */
} print_recipe;
static const print_recipe print_recipes[] = {
    {"Aspect Ratio", {"4:3", "16:9"}, {0, 1},
     {"graphics.aspect_ratio=0", "graphics.aspect_ratio=1"},
     "option", "OptionRoot", "", "", "1.5", {0, 0, 1, 1}},
    {"Controller Tiles", {"ORIGINAL", "BY PLAYER"}, {0, 1},
     {"interface.controller_tiles=0", "interface.controller_tiles=1"},
     "option", "OptionRoot", "down,down,down,down,down", "0.5", "1.5", {0, .4f, .8f, 1}},
    {"Desktop Input", {"OFF", "ON"}, {0, 1},
     {"controls.desktop_menu=0", "controls.desktop_menu=1"},
     "option", "OptionRoot", "mouse:0.4:0.45", "", "2", {.22f, .27f, .6f, .62f}},
    {"Hover Feedback", {"OFF", "ON"}, {0, 1},
     {"interface.mouse_hover_feedback=0\ncontrols.desktop_menu=1",
      "interface.mouse_hover_feedback=1\ncontrols.desktop_menu=1"},
     "option", "OptionRoot", "mouse:0.3:0.45", "", "2", {.08f, .3f, .46f, .65f}},
    {"UI Filter", {"ORIGINAL", "SMOOTH"}, {2, 0},
     {"graphics.ui_filter=2", "graphics.ui_filter=0"},
     "option", "OptionRoot", "", "", "1.5", {.03f, .2f, .19f, .37f}},
    {"Text Clarity", {"0%", "100%"}, {0, 4},
     {"graphics.text_sharpness=0", "graphics.text_sharpness=100"},
     "option", "OptionRoot", "", "", "1.5", {.03f, .2f, .19f, .37f}},
};
#define PRINT_RECIPES (int)(sizeof print_recipes / sizeof *print_recipes)
static struct {
    int started;
    SDL_Thread *thread;
    SDL_mutex *lock;
    SDL_atomic_t quit, want; /* want: the recipe on screen, -1 none */
    int state[PRINT_RECIPES][2]; /* 0 coming, 1 pixels ready, 2 shown, -1 none */
    uint8_t *rgba[PRINT_RECIPES][2];
    int w[PRINT_RECIPES][2], h[PRINT_RECIPES][2];
    plat_texture *image[PRINT_RECIPES][2];
    char dir[LAUNCHER_PATH], exe[LAUNCHER_PATH], disc[LAUNCHER_PATH], api[16];
} launcher_prints;
/* the recipe for a row, -1 none */
static int launcher_prints_recipe(int id)
{
    if (id < 0 || id >= video_setting_count())
        return -1;
    for (int r = 0; r < PRINT_RECIPES; r++)
        if (!strcmp(video_setting(id)->label, print_recipes[r].label))
            return r;
    return -1;
}
#ifdef LAUNCHER_PRINTS
static void prints_path(char *out, size_t size, int r, int k)
{
    char name[48];
    snprintf(name, sizeof name, "%s", print_recipes[r].label);
    for (char *c = name; *c; c++)
        *c = *c == ' ' ? '-' : (char)tolower((unsigned char)*c);
    snprintf(out, size, "%s/%s-%d.png", launcher_prints.dir, name, k);
}
static int prints_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f)
        fclose(f);
    return f != NULL;
}
/* runs the game for one picture; 1 when it wrote it */
static int prints_take(int r, int k, const char *png)
{
    const print_recipe *p = &print_recipes[r];
    char cfg[LAUNCHER_PATH + 16];
    snprintf(cfg, sizeof cfg, "%s/options.ini", launcher_prints.dir);
    FILE *f = fopen(cfg, "wb");
    if (!f)
        return 0;
    fprintf(f, "preset=Enhanced\ngraphics.renderer=%s\n%s\n",
            launcher_prints.api[0] ? launcher_prints.api : "opengl", p->config[k]);
    fclose(f);
    remove(png);
    char overlay[LAUNCHER_PATH + 16];
    snprintf(overlay, sizeof overlay, "%s/no-mods", launcher_prints.dir); /* no mods in a print */
#ifdef _WIN32
    char line[4 * LAUNCHER_PATH + 256];
    int n = snprintf(line, sizeof line,
                     "\"%s\" \"%s\" %s %s --config-file \"%s\" --shot \"%s\" --shot-delay %s",
                     launcher_prints.exe, launcher_prints.disc, p->menu, p->root, cfg, png, p->delay);
    if (*p->inputs)
        n += snprintf(line + n, sizeof line - n, " --input %s", p->inputs);
    if (*p->interval)
        snprintf(line + n, sizeof line - n, " --input-interval %s", p->interval);
    wchar_t command[4 * LAUNCHER_PATH + 256];
    if (!MultiByteToWideChar(CP_UTF8, 0, line, -1, command, (int)(sizeof command / sizeof *command)))
        return 0;
    /* the environment: the player's, hidden, mute and without mods */
    wchar_t *base = GetEnvironmentStringsW(), *env = NULL;
    size_t used = 0, room = 0;
    char extra[3][LAUNCHER_PATH + 32];
    snprintf(extra[0], sizeof extra[0], "MOON_WINDOW_HIDDEN=1");
    snprintf(extra[1], sizeof extra[1], "SDL_AUDIODRIVER=dummy");
    snprintf(extra[2], sizeof extra[2], "MOON_OVERLAY=%s", overlay);
    for (const wchar_t *e = base; e && *e; e += wcslen(e) + 1)
        room += wcslen(e) + 1;
    room += 3 * (LAUNCHER_PATH + 32) + 2;
    env = malloc(room * sizeof *env);
    for (const wchar_t *e = base; env && e && *e; e += wcslen(e) + 1) {
        if (!_wcsnicmp(e, L"MOON_WINDOW_HIDDEN=", 19) || !_wcsnicmp(e, L"SDL_AUDIODRIVER=", 16) ||
            !_wcsnicmp(e, L"MOON_OVERLAY=", 13) || !_wcsnicmp(e, L"MOONSTUDIO_", 11))
            continue;
        memcpy(env + used, e, (wcslen(e) + 1) * sizeof *env);
        used += wcslen(e) + 1;
    }
    for (int i = 0; env && i < 3; i++)
        used += MultiByteToWideChar(CP_UTF8, 0, extra[i], -1, env + used, (int)(room - used - 1));
    if (env)
        env[used] = 0;
    if (base)
        FreeEnvironmentStringsW(base);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    int ran = env && CreateProcessW(NULL, command, NULL, NULL, FALSE,
                                    CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
                                        BELOW_NORMAL_PRIORITY_CLASS,
                                    env, NULL, &si, &pi);
    free(env);
    if (!ran)
        return 0;
    CloseHandle(pi.hThread);
    for (int waited = 0; WaitForSingleObject(pi.hProcess, 200) == WAIT_TIMEOUT; waited += 200)
        if (SDL_AtomicGet(&launcher_prints.quit) || waited > 120000) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            break;
        }
    CloseHandle(pi.hProcess);
#else
    char *argv[16];
    int a = 0;
    argv[a++] = launcher_prints.exe, argv[a++] = launcher_prints.disc;
    argv[a++] = (char *)p->menu, argv[a++] = (char *)p->root;
    argv[a++] = "--config-file", argv[a++] = cfg, argv[a++] = "--shot", argv[a++] = (char *)png;
    argv[a++] = "--shot-delay", argv[a++] = (char *)p->delay;
    if (*p->inputs)
        argv[a++] = "--input", argv[a++] = (char *)p->inputs;
    if (*p->interval)
        argv[a++] = "--input-interval", argv[a++] = (char *)p->interval;
    argv[a] = NULL;
    pid_t pid = fork();
    if (pid == 0) {
        setenv("MOON_WINDOW_HIDDEN", "1", 1);
        setenv("SDL_AUDIODRIVER", "dummy", 1);
        setenv("MOON_OVERLAY", overlay, 1);
        execv(launcher_prints.exe, argv);
        _exit(127);
    }
    if (pid < 0)
        return 0;
    int status = 0;
    for (int waited = 0; waitpid(pid, &status, WNOHANG) == 0; waited += 200) {
        if (SDL_AtomicGet(&launcher_prints.quit) || waited > 120000) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        SDL_Delay(200);
    }
#endif
    return prints_exists(png);
}
/* reads a print, keeps the recipe's crop, no wider than PRINTS_WIDTH */
static void prints_read(int r, int k, const char *png)
{
    int w = 0, h = 0, n = 0;
    uint8_t *pixels = stbi_load(png, &w, &h, &n, 4), *out = NULL;
    const float *c = print_recipes[r].crop;
    int x0 = (int)(c[0] * w), y0 = (int)(c[1] * h), cw = (int)((c[2] - c[0]) * w),
        ch = (int)((c[3] - c[1]) * h);
    int step = cw > PRINTS_WIDTH ? (cw + PRINTS_WIDTH - 1) / PRINTS_WIDTH : 1;
    int ow = cw / step, oh = ch / step;
    if (pixels && ow > 0 && oh > 0 && (out = malloc((size_t)ow * oh * 4)) != NULL)
        for (int y = 0; y < oh; y++)
            for (int x = 0; x < ow; x++)
                for (int ch4 = 0; ch4 < 4; ch4++) {
                    unsigned sum = 0;
                    for (int j = 0; j < step; j++)
                        for (int i = 0; i < step; i++)
                            sum += pixels[((size_t)(y0 + y * step + j) * w + x0 + x * step + i) * 4 +
                                          ch4];
                    out[((size_t)y * ow + x) * 4 + ch4] = (uint8_t)(sum / (step * step));
                }
    if (pixels)
        stbi_image_free(pixels);
    SDL_LockMutex(launcher_prints.lock);
    launcher_prints.rgba[r][k] = out;
    launcher_prints.w[r][k] = ow, launcher_prints.h[r][k] = oh;
    launcher_prints.state[r][k] = out ? 1 : -1;
    SDL_UnlockMutex(launcher_prints.lock);
}
static int prints_worker(void *unused)
{
    (void)unused;
    char path[LAUNCHER_PATH + 64], stamp[16] = "";
    gt4_save_mkdir(launcher_prints.dir);
    snprintf(path, sizeof path, "%s/version", launcher_prints.dir);
    FILE *f = fopen(path, "rb");
    if (f) {
        if (!fgets(stamp, sizeof stamp, f))
            stamp[0] = 0;
        fclose(f);
    }
    if (strcmp(stamp, PRINTS_VERSION)) { /* another recipe book: every print again */
        for (int r = 0; r < PRINT_RECIPES; r++)
            for (int k = 0; k < 2; k++) {
                char png[LAUNCHER_PATH + 64];
                prints_path(png, sizeof png, r, k);
                remove(png);
            }
        if ((f = fopen(path, "wb")) != NULL) {
            fputs(PRINTS_VERSION, f);
            fclose(f);
        }
    }
    for (;;) {
        /* the row on screen first, then the rest in order */
        int pick = -1, want = SDL_AtomicGet(&launcher_prints.want);
        SDL_LockMutex(launcher_prints.lock);
        for (int i = -1; i < 2 * PRINT_RECIPES && pick < 0; i++) {
            int r = i < 0 ? want : i / 2;
            for (int k = 0; k < 2 && pick < 0 && r >= 0; k++)
                if (!launcher_prints.state[r][k] && !launcher_prints.rgba[r][k])
                    pick = r * 2 + k;
        }
        SDL_UnlockMutex(launcher_prints.lock);
        if (pick < 0 || SDL_AtomicGet(&launcher_prints.quit))
            break;
        int r = pick / 2, k = pick % 2;
        char png[LAUNCHER_PATH + 64];
        prints_path(png, sizeof png, r, k);
        int kept = prints_exists(png);
        if (kept || prints_take(r, k, png)) {
            printf("studio: print %s (%s) %s\n", print_recipes[r].label, print_recipes[r].names[k],
                   kept ? "from the cache" : "taken");
            prints_read(r, k, png);
        } else {
            printf("studio: no print for %s (%s)\n", print_recipes[r].label, print_recipes[r].names[k]);
            SDL_LockMutex(launcher_prints.lock);
            launcher_prints.state[r][k] = -1;
            SDL_UnlockMutex(launcher_prints.lock);
        }
    }
    return 0;
}
#endif
/* Each frame while OPTIONS is open: starts the prints the first time, says which row is on
   screen and hands finished pictures to the renderer. */
static void launcher_prints_step(int id)
{
#ifdef LAUNCHER_PRINTS
    SDL_AtomicSet(&launcher_prints.want, launcher_prints_recipe(id));
    if (!launcher_prints.started) {
        launcher_prints.started = 1;
        if (!studio_disc || !studio_prefs || !studio_prefs->file[0])
            goto none;
        snprintf(launcher_prints.disc, sizeof launcher_prints.disc, "%s", studio_disc);
        snprintf(launcher_prints.api, sizeof launcher_prints.api, "%s", studio_prefs->api);
        snprintf(launcher_prints.dir, sizeof launcher_prints.dir, "%s", studio_prefs->file);
        char *cut = strrchr(launcher_prints.dir, '/'), *back = strrchr(launcher_prints.dir, '\\');
        if (back && (!cut || back > cut))
            cut = back;
        if (cut)
            *cut = 0;
        else
            snprintf(launcher_prints.dir, sizeof launcher_prints.dir, ".");
        strncat(launcher_prints.dir, "/launcher-prints",
                sizeof launcher_prints.dir - strlen(launcher_prints.dir) - 1);
#ifdef _WIN32
        wchar_t self[LAUNCHER_PATH];
        DWORD got = GetModuleFileNameW(NULL, self, LAUNCHER_PATH);
        if (!got || got >= LAUNCHER_PATH ||
            !WideCharToMultiByte(CP_UTF8, 0, self, -1, launcher_prints.exe, sizeof launcher_prints.exe,
                                 NULL, NULL))
            goto none;
#else
        ssize_t got = readlink("/proc/self/exe", launcher_prints.exe, sizeof launcher_prints.exe - 1);
        if (got <= 0)
            goto none;
        launcher_prints.exe[got] = 0;
#endif
        launcher_prints.lock = SDL_CreateMutex();
        launcher_prints.thread =
            launcher_prints.lock ? SDL_CreateThread(prints_worker, "launcher prints", NULL) : NULL;
        if (!launcher_prints.thread)
            goto none;
    }
    if (!launcher_prints.lock)
        return;
    SDL_LockMutex(launcher_prints.lock);
    for (int r = 0; r < PRINT_RECIPES; r++)
        for (int k = 0; k < 2; k++)
            if (launcher_prints.state[r][k] == 1) {
                launcher_prints.image[r][k] = plat_texture_create(
                    launcher_prints.w[r][k], launcher_prints.h[r][k], launcher_prints.rgba[r][k]);
                free(launcher_prints.rgba[r][k]);
                launcher_prints.rgba[r][k] = NULL;
                launcher_prints.state[r][k] = launcher_prints.image[r][k] ? 2 : -1;
            }
    SDL_UnlockMutex(launcher_prints.lock);
    return;
none:
    for (int r = 0; r < PRINT_RECIPES; r++)
        launcher_prints.state[r][0] = launcher_prints.state[r][1] = -1;
#else
    (void)id;
    for (int r = 0; r < PRINT_RECIPES; r++)
        launcher_prints.state[r][0] = launcher_prints.state[r][1] = -1;
#endif
}
/* a shot waits while the row's prints are still coming */
static int launcher_prints_busy(int id)
{
    int r = launcher_prints_recipe(id);
    return r >= 0 && (launcher_prints.state[r][0] < 2 && launcher_prints.state[r][0] >= 0 ||
                      launcher_prints.state[r][1] < 2 && launcher_prints.state[r][1] >= 0);
}
/* the launcher closes: the game being printed stops, the pictures go */
static void launcher_prints_close(void)
{
#ifdef LAUNCHER_PRINTS
    SDL_AtomicSet(&launcher_prints.quit, 1);
    if (launcher_prints.thread)
        SDL_WaitThread(launcher_prints.thread, NULL);
    for (int r = 0; r < PRINT_RECIPES; r++)
        for (int k = 0; k < 2; k++)
            free(launcher_prints.rgba[r][k]);
    if (launcher_prints.lock)
        SDL_DestroyMutex(launcher_prints.lock);
#endif
    for (int r = 0; r < PRINT_RECIPES; r++)
        for (int k = 0; k < 2; k++)
            plat_texture_destroy(launcher_prints.image[r][k]);
    memset(&launcher_prints, 0, sizeof launcher_prints);
}
