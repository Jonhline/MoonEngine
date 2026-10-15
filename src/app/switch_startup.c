#include "launcher.h"
#include "save.h"
#include "switch_fpu.h"
#include "phys.h"
#include <SDL.h>
#include <stdio.h>
#include <sys/stat.h>

int launcher_bootstrap(int *argc, char ***argv, launcher_startup *start)
{
    const char *root = "sdmc:/switch/opengt4";
    mkdir("sdmc:/switch", 0777);
    if (!gt4_save_mkdir(root))
        return -1;
    FILE *log = freopen("sdmc:/switch/opengt4/opengt4.log", "w", stderr);
    (void)log;
    /* Keep complete diagnostic records available after HOME -> Close. */
    setvbuf(stderr, NULL, _IOLBF, 8192);
    gt4_phys_fpu_enter();
    if (!opengt4_switch_fpu_check())
        return -1;
    if (!launcher_preferences_load(&start->preferences, "sdmc:/switch/opengt4/config.ini"))
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
    start->arguments[0] = *argc > 0 && *argv ? (*argv)[0] : "opengt4";
    start->arguments[1] = start->preferences.disc;
    start->arguments[2] = "--save-dir";
    start->arguments[3] = start->preferences.saves;
    start->arguments[4] = "--config-file";
    start->arguments[5] = start->video_file;
    start->arguments[6] = NULL;
    *argc = 6;
    *argv = start->arguments;
    /* SDL normalizes Nintendo face buttons to their physical positions. */
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    /* Reuse the existing five-second pose trace for disc/attract replays.
       This is diagnostic output, not a simulation tick or renderer setting. */
    SDL_setenv("OPENGT4_RACE_REPLAY_LOG", "1", 0);
    return 1;
}

int launcher_capture_gallery(const char *disc, const char *folder)
{
    (void)disc;
    (void)folder;
    return 1;
}
