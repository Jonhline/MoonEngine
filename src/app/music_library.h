#ifndef MOON_MUSIC_LIBRARY_H
#define MOON_MUSIC_LIBRARY_H
#include "options_store.h"
#include <stdint.h>
#define ML_LISTS 16
#define ML_TRACKS 256
#define ML_PATH 1024
enum { ML_MENU, ML_RACE, ML_REPLAY, ML_SLIDESHOW, ML_LAUNCHER, ML_CONTEXTS };
typedef struct {
    char path[ML_PATH], title[128];
    int enabled;
} ml_track;
typedef struct {
    char name[64];
    int count, shuffle, catalog;
    ml_track tracks[ML_TRACKS];
} ml_playlist;
typedef struct music_library music_library;
music_library *music_library_open(moon_options *store);
void music_library_disc(music_library *m, const char *disc);
int music_library_catalog(music_library *m);
int music_library_replace(music_library *m, int list, const ml_track *tracks, int count,
                          int shuffle);
void music_library_close(music_library *m);
void music_library_install(music_library *m);
void music_library_sync(music_library *m);
unsigned music_library_played(music_library *m);
int music_library_peak(music_library *m);
int music_library_enabled(music_library *m);
int music_library_enable(music_library *m, int enabled);
int music_library_count(music_library *m);
const ml_playlist *music_library_list(music_library *m, int list);
int music_library_create(music_library *m, const char *name);
int music_library_rename(music_library *m, int list, const char *name);
int music_library_delete(music_library *m, int list);
int music_library_duplicate(music_library *m, int list);
int music_library_add(music_library *m, int list, const char *path, const char *title);
int music_library_remove(music_library *m, int list, int track);
int music_library_move(music_library *m, int list, int track, int delta);
int music_library_track_rename(music_library *m, int list, int track, const char *title);
int music_library_track_enabled(music_library *m, int list, int track);
int music_library_shuffle(music_library *m, int list);
int music_library_jukebox_commit(music_library *, int list, const int *order, const int *enabled,
                                 int count, int shuffle);
int music_library_assign(music_library *m, int context, int list);
int music_library_assignment(music_library *m, int context);
int music_library_import(music_library *m, const char *path);
int music_library_export(music_library *m, int list, const char *path);
void music_library_preview(music_library *m, int list, int track); /* -1 stops */
int music_library_preview_playing(music_library *m);
void music_library_status(music_library *m, char *out, int bytes);
int music_library_available(const char *path);
/* Source substitution only; zero preserves the original music path. */
int music_library_render(music_library *, int context, int16_t *out, int frames, int gain,
                         int paused);
#endif
