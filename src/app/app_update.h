#ifndef MOON_APP_UPDATE_H
#define MOON_APP_UPDATE_H
/* P: stable builds check the project's GitHub releases for a newer version
   (Windows). Nothing in the game reads this. Dev builds never check.

   start: removes what a previous update left behind, then, when enabled, looks
   up the latest release in the background; a newer one is downloaded, its
   SHA-256 checked against the release's digest and its files unpacked beside
   the program as *.moon-new. finish (at quit): the unpacked files replace the
   program's, the running ones renamed *.moon-old (removed on the next start).
   status: a short line for the home screen, or NULL when there is nothing to say. */
void app_update_start(int enabled);
const char *app_update_status(void);
void app_update_finish(void);

/* Exposed for tests: the release JSON reduced to what the update needs. */
typedef struct {
    char version[32], url[1024], sha256[65];
    unsigned long long size;
} app_update_release;
int app_update_parse_release(const char *json, const char *asset_suffix, app_update_release *out);
/* async job for the studio (state: 0 idle, 1 checking, 2 downloading, 3 ready, 4 none, 5 error) */
void app_update_begin(int auto_check);
int app_update_state(void);
int app_update_progress(void);
const char *app_update_message(void);
int app_update_available(char *ver, size_t n);
int app_update_download(void);
#endif
