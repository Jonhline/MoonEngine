#ifndef MOON_LAUNCHER_H
#define MOON_LAUNCHER_H
#include "pad_config.h"
#include <stddef.h>
#define LAUNCHER_PATH 1024
/* P: PC preparation independent of every disc asset. Layout/system fonts are
   PC choices, not reconstructed PS2 artwork. No simulation changes. */
typedef struct {
    char disc[LAUNCHER_PATH],saves[LAUNCHER_PATH],file[LAUNCHER_PATH];
    char api[16];   /* the graphics API the first-run wizard saved ("" = not chosen yet) */
    char car[48];   /* the room's car code for the front-end ("" = the built-in default) */
    int color;      /* its paint index */
    int show;
    int theme;      /* the launcher room: 0 light, 1 dark */
    int mood;       /* the room's light colour (an index into the launcher's list) */
    char scene[32]; /* what stands behind the car: "" the room, else a course (/crs) */
    char channel[16]; /* release channel: "stable" (default) or "nightly" */
} launcher_preferences;
typedef struct {
    int width,height,fullscreen,vsync,upscale,filter,text,aspect;
    int volume[7];
    gt4_pad_config pad[2];
} launcher_options;
typedef struct {
    launcher_preferences preferences;
    char video_file[LAUNCHER_PATH];
    char *arguments[7];
    /* for whoever has to look at this without a mouse: open a page straight away
       and write one frame to a PNG, then quit (`--launcher-page mods --launcher-shot f.png`). */
    char page[32];
    char shot[LAUNCHER_PATH];
    int install_zips;   /* --launcher-install: unpack every archive in the mods folder now */
} launcher_startup;
extern const char *const launcher_volume_keys[7];
void launcher_options_defaults(launcher_options *o);
int launcher_options_load(launcher_options *o,const char *video,const char *saves);
/* Groups: 1 video, 2 audio, 4 pad 1P, 8 pad 2P. Preserve other options and
   merge only edited options into active careers through native save APIs. */
int launcher_options_save(const launcher_options *o,const char *video,const char *saves,int groups);
int launcher_preferences_load(launcher_preferences *p,const char *file);
int launcher_preferences_save(const launcher_preferences *p);
int launcher_validate_disc(const char *path,char *message,size_t size);
int launcher_capture_gallery(const char *disc,const char *folder);
/* The game CLI (a disc, --studio, --shot) returns 1 immediately, untouched. No args or
   --launcher is the studio front-end (the in-engine wizard, then the room); --launcher-v2
   is our SDL options screen. No window is opened here. Returns 1 = run the game,
   2 = front-end, 0 = cancel, -1 = failure. */
int launcher_bootstrap(int *argc,char ***argv,launcher_startup *startup);
#endif
