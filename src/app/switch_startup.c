#include "launcher.h"
#include "save.h"
#include "switch_fpu.h"
#include "phys.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <arpa/inet.h>
#include <sys/stat.h>

/* Flush network stdio before libnx removes the socket device during exit. */
static void remote_log_shutdown(void)
{
    fflush(stderr);
    fclose(stdout);
    fclose(stderr);
    socketExit();
}

int launcher_bootstrap(int *argc, char ***argv, launcher_startup *start)
{
    /* P: the folder of the project's former name (disc, saves, settings)
       keeps working while no Moon Engine folder exists */
    static char root[64] = "sdmc:/switch/moonengine", file[96];
    struct stat st;
    if (stat(root, &st) && !stat("sdmc:/switch/opengt4", &st))
        snprintf(root, sizeof root, "sdmc:/switch/opengt4");
    mkdir("sdmc:/switch", 0777);
    if (!gt4_save_mkdir(root))
        return -1;
    snprintf(file, sizeof file, "%s/moonengine.log", root);
    /* A host override makes multi-adapter PCs (VPN plus LAN) deterministic. */
    for (int i = 1; i < *argc; i++) {
        if (strcmp((*argv)[i], "--nxlink-host"))
            continue;
        if (i + 1 >= *argc || !inet_aton((*argv)[i + 1], &__nxlink_host))
            return -1;
        memmove(*argv + i, *argv + i + 2, (size_t)(*argc - i - 1) * sizeof **argv);
        *argc -= 2;
        i--;
    }
    int remote_log = -1;
    /* nxlink supplies its host address only for a netloader launch. */
    if (__nxlink_host.s_addr && R_SUCCEEDED(socketInitializeDefault())) {
        remote_log = nxlinkStdio();
        if (remote_log < 0)
            socketExit();
        else
            atexit(remote_log_shutdown);
    }
    if (remote_log < 0) {
        FILE *log = freopen(file, "w", stderr);
        (void)log;
    }

    /* Keep complete diagnostic records available after HOME -> Close. */
    setvbuf(stderr, NULL, _IOLBF, 8192);
    if (remote_log >= 0)
        setvbuf(stdout, NULL, _IOLBF, 4096);
    gt4_phys_fpu_enter();
    if (!moon_switch_fpu_check())
        return -1;
    snprintf(file, sizeof file, "%s/config.ini", root);
    if (!launcher_preferences_load(&start->preferences, file))
        return -1;
    if (!start->preferences.disc[0])
        snprintf(start->preferences.disc, LAUNCHER_PATH, "%s/gt4.iso", root);
    if (!start->preferences.saves[0])
        snprintf(start->preferences.saves, LAUNCHER_PATH, "%s", root);
    start->preferences.show = 0;
    FILE *config = fopen(start->preferences.file, "r");
    if (config)
        fclose(config);
    else if (!launcher_preferences_save(&start->preferences))
        return -1;
    snprintf(start->video_file, LAUNCHER_PATH, "%s", start->preferences.file);
    start->arguments[0] = *argc > 0 && *argv ? (*argv)[0] : "moonengine";
    start->arguments[1] = start->preferences.disc;
    start->arguments[2] = "--save-dir";
    start->arguments[3] = start->preferences.saves;
    start->arguments[4] = "--config-file";
    start->arguments[5] = start->video_file;
    start->arguments[6] = NULL;
    /* Keep netloader command-line fixtures, such as --drive and --race-course. */
    char **arguments = calloc((size_t)*argc + 6, sizeof *arguments);
    if (!arguments)
        return -1;
    memcpy(arguments, start->arguments, 6 * sizeof *arguments);
    for (int i = 1; i < *argc; i++)
        arguments[i + 5] = (*argv)[i];
    *argc += 5;
    *argv = arguments; /* The startup arguments remain alive for the application lifetime. */
    /* SDL normalizes Nintendo face buttons to their physical positions. */
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
#ifdef MOON_SWITCH_RACE_DIAGNOSTICS
    /* Position traces use synchronous stdio: keep them out of production races. */
    SDL_setenv("MOON_RACE_REPLAY_LOG", "1", 0);
#endif
    return 1;
}

int launcher_capture_gallery(const char *disc, const char *folder)
{
    (void)disc;
    (void)folder;
    return 1;
}
