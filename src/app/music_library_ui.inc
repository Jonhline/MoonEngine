/* P: local file operations added to the disc's actual slide/MusicRoot Jukebox. */
#include "pcm_music.h"
#include <stdarg.h>

/* Diagnostics survive a GUI launch: same append/flush/cap pattern as presence.log. */
static void mlj_log(const char *fmt, ...)
{
    char pref[1200], path[1300], line[512];
    if (!plat_pref_dir(pref, sizeof pref))
        return;
    snprintf(path, sizeof path, "%sjukebox.log", pref);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    FILE *f = fopen(path, "rb");
    long size = 0;
    if (f) {
        if (!fseek(f, 0, SEEK_END))
            size = ftell(f);
        fclose(f);
    }
    f = fopen(path, size > 64 * 1024 ? "wb" : "ab");
    if (!f)
        return;
    time_t now = time(NULL);
    struct tm date;
    memset(&date, 0, sizeof date);
#ifdef _WIN32
    int ok = !localtime_s(&date, &now);
#else
    int ok = localtime_r(&now, &date) != NULL;
#endif
    if (ok)
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d %s\n", date.tm_year + 1900, date.tm_mon + 1,
                date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, line);
    else
        fprintf(f, "%s\n", line);
    fflush(f);
    fclose(f);
}
/* The one Jukebox is the disc's slide/MusicRoot reached from GT Mode. With local
   songs, its Library lists them beside the disc's; its own scripts edit Racing and
   Slideshow, its own sound.startStreamEx plays them, its own Exit leaves. */
typedef struct {
    music_library *library;
    menu *page;
    int race, slide, catalog;
    int folder[2]; /* P: Menu and Replay folders, -1 without audio.jukebox_folders */
    unsigned serial;
    SDL_atomic_t playing, paused, volume;
} ml_jukebox;
static ml_jukebox app_jukebox;
static void app_music_ensure(int force)
{
    if (!app_music_library && app_video_settings &&
        (force || moon_options_int(&app_video_settings->store, "audio.custom_music", 0, 0, 1))) {
        app_music_library = music_library_open(&app_video_settings->store);
        music_library_install(app_music_library);
        const char *disc = studio_disc ? studio_disc : presentation_disc;
        if (disc)
            music_library_disc(app_music_library, disc);
    }
}
/* P: audio.jukebox_folders adds Menu and Replay to the Jukebox's folders. */
static int mlj_folders_on(void)
{
    return app_video_settings &&
           moon_options_int(&app_video_settings->store, "audio.jukebox_folders",
                            strcmp(app_video_settings->store.preset, "Classic") != 0, 0, 1);
}
/* P: audio.jukebox_groups lists the Music folder's subfolders together. */
static int mlj_groups_on(void)
{
    return app_video_settings &&
           moon_options_int(&app_video_settings->store, "audio.jukebox_groups",
                            strcmp(app_video_settings->store.preset, "Classic") != 0, 0, 1);
}
static char mlj_music_root[ML_PATH];
/* a file's folder under Music ("" at its top or elsewhere), '/' between levels */
static void mlj_group(const char *path, char *out, size_t size)
{
    size_t n = strlen(mlj_music_root);
    *out = 0;
    if (!n || SDL_strncasecmp(path, mlj_music_root, n) || (path[n] != '/' && path[n] != '\\'))
        return;
    snprintf(out, size, "%s", path + n + 1);
    char *end = NULL;
    for (char *c = out; *c; c++)
        if (*c == '/' || *c == '\\') {
            *c = '/';
            end = c;
        }
    if (end)
        *end = 0;
    else
        *out = 0;
}
static const char *mlj_shown_tip(const ui_widget *w);
static const char *mlj_find_text(ui_widget *w, const char *needle)
{
    if (!w) return NULL;
    if (w->text && strstr(w->text, needle))
        return w->text;
    for (int i = 0; i < w->nkids; i++) {
        const char *t = mlj_find_text(w->kids[i], needle);
        if (t) return t;
    }
    return NULL;
}
static int mlj_category_id(menu *m, char *name, size_t size)
{
    ui_widget *root = menu_jukebox_page_widget(m);
    if (!root) return -1;
    const char *t = mlj_find_text(root, "No. of songs");
    if (t && getenv("MLJ_DUMP")) fprintf(stderr, "jukebox category text: %s\n", t);
    if (!t) return -1;
    if (strstr(t, "Racing")) snprintf(name, size, "Race");
    else if (strstr(t, "Slideshow")) snprintf(name, size, "Slideshow");
    else if (strstr(t, "Menu")) snprintf(name, size, "Menu");
    else if (strstr(t, "Replay")) snprintf(name, size, "Replay");
    else return -1;
    if (!strcmp(name, "Race")) return app_jukebox.race;
    if (!strcmp(name, "Slideshow")) return app_jukebox.slide;
    if (!strcmp(name, "Menu") && app_jukebox.folder[0] >= 0) return app_jukebox.folder[0];
    if (!strcmp(name, "Replay") && app_jukebox.folder[1] >= 0) return app_jukebox.folder[1];
    return -1;
}

static const ml_playlist *mlj_sorting;
static int mlj_shown[ML_TRACKS]; /* the Library's rows, catalog indices */
static void mlj_clean_title(const char *in, char *out, size_t size)
{
    size_t n = 0;
    for (; *in && n + 1 < size; in++) {
        if (!strncmp(in, " / WARNING", 10) || !strncmp(in, " WARNING_", 9) ||
            !strncmp(in, " [youtube", 9) || !strncmp(in, "yt-dlp", 6) ||
            !strncmp(in, "WARNING_", 8))
            break;
        out[n++] = *in;
    }
    out[n] = 0;
    while (n && (out[n - 1] == ' ' || out[n - 1] == '/')) out[--n] = 0;
}
static int mlj_group_order(const void *a, const void *b)
{
    const ml_track *x = &mlj_sorting->tracks[*(const int *)a],
                   *y = &mlj_sorting->tracks[*(const int *)b];
    char gx[ML_PATH], gy[ML_PATH];
    mlj_group(x->path, gx, sizeof gx);
    mlj_group(y->path, gy, sizeof gy);
    int c = SDL_strcasecmp(gx, gy);
    if (!c)
        c = SDL_strcasecmp(x->title, y->title);
    return c ? c : *(const int *)a - *(const int *)b;
}
static void app_music_tick(void)
{
    /* The Jukebox holds the library while its page is up, whatever the option. */
    if (app_music_library && app_video_settings && !app_jukebox.page &&
        !moon_options_int(&app_video_settings->store, "audio.custom_music", 0, 0, 1)) {
        music_library_install(NULL);
        music_library_close(app_music_library);
        app_music_library = NULL;
    } else {
        app_music_ensure(0);
        music_library_sync(app_music_library);
    }
}
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif
static int ml_add_folder(music_library *m, int list, const char *folder, int depth, int *scanned)
{
#ifdef _WIN32
    if (depth > 8 || *scanned > 4096 || !music_library_list(m, list) ||
        music_library_list(m, list)->count >= ML_TRACKS)
        return 0;
    wchar_t path[4096], pattern[4096];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, folder, -1, path, 4096))
        return 0;
    if (_snwprintf(pattern, 4096, L"%ls\\*", path) < 0)
        return 0;
    WIN32_FIND_DATAW item;
    HANDLE find = FindFirstFileW(pattern, &item);
    if (find == INVALID_HANDLE_VALUE)
        return 0;
    int added = 0;
    do {
        if (!wcscmp(item.cFileName, L".") || !wcscmp(item.cFileName, L".."))
            continue;
        if (++*scanned > 4096)
            break;
        wchar_t wide[4096];
        char full[ML_PATH];
        if (_snwprintf(wide, 4096, L"%ls\\%ls", path, item.cFileName) < 0 ||
            !WideCharToMultiByte(CP_UTF8, 0, wide, -1, full, sizeof full, NULL, NULL))
            continue;
        if (item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(item.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                added += ml_add_folder(m, list, full, depth + 1, scanned);
        } else {
            const char *dot = strrchr(full, '.');
            if (dot && (!SDL_strcasecmp(dot, ".wav") || !SDL_strcasecmp(dot, ".mp3") ||
                        !SDL_strcasecmp(dot, ".flac") || !SDL_strcasecmp(dot, ".ogg") ||
                        !SDL_strcasecmp(dot, ".m4a")))
                added += music_library_add(m, list, full, NULL);
        }
        if (music_library_list(m, list)->count >= ML_TRACKS)
            break;
    } while (FindNextFileW(find, &item));
    FindClose(find);
    return added;
#else
    if (depth > 8 || *scanned >= 4096)
        return 0;
    DIR *dir = opendir(folder);
    if (!dir)
        return 0;
    int added = 0;
    struct dirent *item;
    while ((item = readdir(dir)) && *scanned < 4096) {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, ".."))
            continue;
        ++*scanned;
        char full[ML_PATH];
        struct stat st;
        if (snprintf(full, sizeof full, "%s/%s", folder, item->d_name) >= sizeof full ||
            lstat(full, &st))
            continue;
        if (S_ISDIR(st.st_mode))
            added += ml_add_folder(m, list, full, depth + 1, scanned);
        else if (S_ISREG(st.st_mode)) {
            const char *dot = strrchr(full, '.');
            if (dot && (!SDL_strcasecmp(dot, ".wav") || !SDL_strcasecmp(dot, ".mp3") ||
                        !SDL_strcasecmp(dot, ".flac") || !SDL_strcasecmp(dot, ".ogg") ||
                        !SDL_strcasecmp(dot, ".m4a")))
                added += music_library_add(m, list, full, NULL);
        }
        if (music_library_list(m, list)->count >= ML_TRACKS)
            break;
    }
    closedir(dir);
    return added;
#endif
}
static int mlj_find(const ml_playlist *p, const char *path)
{
    for (int i = 0; p && i < p->count; i++)
        if (!SDL_strcasecmp(p->tracks[i].path, path))
            return i;
    return -1;
}
static int mlj_default_list(ml_jukebox *j, gt4_vol *vol, int context, const char *name)
{
    /* The page's Racing and Slideshow are always these two lists. Another one assigned
       here (an earlier editor's own playlist) is left in the file, not used. */
    for (int i = 0; i < music_library_count(j->library); i++) {
        const ml_playlist *candidate = music_library_list(j->library, i);
        if (!candidate->catalog && !strcmp(candidate->name, name)) {
            if (music_library_assignment(j->library, context) != i)
                music_library_assign(j->library, context, i);
            return i;
        }
    }
    int id = music_library_create(j->library, name);
    if (id < 0)
        return -1;
    gt4_music_playlist original;
    int slide = context == ML_SLIDESHOW;
    if (!gt4_save_option_blob_get(slide ? "slide_playlist_ps2" : "bgm_playlist_ps2", original.bytes,
                                  sizeof original.bytes)) {
        if (slide)
            gt4_music_playlist_slide(&original, vol, 0);
        else
            gt4_music_playlist_default(&original, vol, 0);
    }
    ml_track *tracks = calloc(ML_TRACKS, sizeof *tracks);
    if (!tracks)
        return id;
    const ml_playlist *catalog = music_library_list(j->library, j->catalog);
    /* musiccontrol.h load_playlist: marked rows [dispno, played, marked, playno],
       shown in dispno order (entry byte 11), which is the order the page returns. */
    int marked[128], count = 0, n = 0;
    for (int i = 0; i < original.bytes[0] && i < 128; i++)
        if (original.bytes[16 + i * 16 + 9])
            marked[count++] = i;
    for (int a = 1; a < count; a++)
        for (int b = a; b > 0 && original.bytes[16 + marked[b - 1] * 16 + 11] >
                                     original.bytes[16 + marked[b] * 16 + 11];
             b--) {
            int t = marked[b];
            marked[b] = marked[b - 1];
            marked[b - 1] = t;
        }
    for (int k = 0; k < count && n < ML_TRACKS; k++) {
        int i = marked[k];
        int sequence = gt4_music_playlist_sequence(vol, &original, (unsigned)i);
        char ref[32];
        snprintf(ref, sizeof ref, "gt4:%d", sequence);
        int row = mlj_find(catalog, ref);
        if (row >= 0)
            tracks[n++] = catalog->tracks[row];
    }
    music_library_replace(j->library, id, tracks, n, original.bytes[6]);
    free(tracks);
    music_library_assign(j->library, context, id);
    return id;
}
/* Menu and Replay start empty: the original menu music and, for replays, the
   race list, as the PS2 plays a replay with the race Jukebox. */
static int mlj_folder_list(ml_jukebox *j, const char *name)
{
    for (int i = 0; i < music_library_count(j->library); i++) {
        const ml_playlist *candidate = music_library_list(j->library, i);
        if (!candidate->catalog && !strcmp(candidate->name, name))
            return i;
    }
    return music_library_create(j->library, name);
}
static int mlj_seed(ml_jukebox *j, gt4_vol *vol)
{
    music_library_disc(j->library, gt4_vol_source(vol));
    j->catalog = music_library_catalog(j->library);
    if (j->catalog < 0) {
        mlj_log("jukebox: seed failed, catalog could not be created");
        return 0;
    }
    ml_playlist *combined = calloc(1, sizeof *combined);
    if (!combined) {
        mlj_log("jukebox: seed failed, out of memory");
        return 0;
    }
    const ml_playlist *old = music_library_list(j->library, j->catalog);
    *combined = *old;
    unsigned order[256], n = gt4_music_library_default(vol, order, 256);
    for (unsigned k = 0; k < n && combined->count < ML_TRACKS; k++) {
        char ref[32], info[3][128];
        snprintf(ref, sizeof ref, "gt4:%u", order[k]);
        if (mlj_find(combined, ref) >= 0 || !gt4_pcm_music_info(vol, order[k], info))
            continue;
        ml_track *t = &combined->tracks[combined->count++];
        snprintf(t->path, sizeof t->path, "%s", ref);
        snprintf(t->title, sizeof t->title, "%s", info[0]);
        t->enabled = 1;
    }
    /* Existing personal lists contribute their files to the same library. */
    for (int k = 0; k < music_library_count(j->library); k++) {
        const ml_playlist *list = music_library_list(j->library, k);
        if (k == j->catalog)
            continue;
        for (int i = 0; i < list->count && combined->count < ML_TRACKS; i++)
            if (mlj_find(combined, list->tracks[i].path) < 0) {
                combined->tracks[combined->count] = list->tracks[i];
                combined->tracks[combined->count++].enabled = 1;
            }
    }
    int ok = music_library_replace(j->library, j->catalog, combined->tracks, combined->count, 0);
    free(combined);
    if (!ok) {
        mlj_log("jukebox: seed failed, catalog could not be replaced");
        return 0;
    }
    j->race = mlj_default_list(j, vol, ML_RACE, "Race playlist");
    j->slide = mlj_default_list(j, vol, ML_SLIDESHOW, "Slideshow playlist");
    j->folder[0] = j->folder[1] = -1;
    if (mlj_folders_on()) {
        j->folder[0] = mlj_folder_list(j, "Menu playlist");
        j->folder[1] = mlj_folder_list(j, "Replay playlist");
    }
    if (j->race < 0 || j->slide < 0) {
        mlj_log("jukebox: seed failed, playlist could not be created (race=%d slide=%d)", j->race,
                j->slide);
        return 0;
    }
    return 1;
}
static void mlj_store(ml_jukebox *j)
{
    if (!j->page)
        return;
    const ml_playlist *catalog = music_library_list(j->library, j->catalog);
    ml_track *tracks = calloc(ML_TRACKS, sizeof *tracks);
    if (!tracks)
        return;
    int ids[2] = {j->race, j->slide}, shared = 0;
    const char *empty_names[2] = {"Race", "Slideshow"};
    for (int k = 0; k < 2; k++) {
        char empty_key[64];
        snprintf(empty_key, sizeof empty_key, "audio.jukebox_empty_%s", empty_names[k]);
        if (app_video_settings &&
            moon_options_int(&app_video_settings->store, empty_key, 0, 0, 1))
            continue;
        int order[ML_TRACKS], enabled[ML_TRACKS], shuffle = 0;
        int n = menu_jukebox_playlist_result(j->page, k, order, enabled, ML_TRACKS, &shuffle),
            count = 0;
        if (n < 0 || n != catalog->count)
            continue;
        if (!k)
            shared = shuffle;
        for (int i = 0; i < n; i++)
            if (enabled[order[i]])
                tracks[count++] = catalog->tracks[order[i]];
        const ml_playlist *before = music_library_list(j->library, ids[k]);
        int changed =
            before && (before->count != count || before->shuffle != shuffle ||
                       (count && memcmp(before->tracks, tracks, (size_t)count * sizeof *tracks)));
        if (changed) {
            if (music_library_replace(j->library, ids[k], tracks, count, shuffle)) {
                music_library_assign(j->library, k ? ML_SLIDESHOW : ML_RACE, ids[k]);
                music_library_enable(j->library, 1);
            }
        }
    }
    /* Menu and Replay: one play mode for every folder, as the page has */
    for (int k = 0; k < 2 && j->folder[0] >= 0; k++) {
        const char *folder_name = k ? "Replay" : "Menu";
        char empty_key[64];
        snprintf(empty_key, sizeof empty_key, "audio.jukebox_empty_%s", folder_name);
        if (app_video_settings &&
            moon_options_int(&app_video_settings->store, empty_key, 0, 0, 1))
            continue;
        int rows[ML_TRACKS], n = menu_jukebox_folder_result(j->page, k, rows, ML_TRACKS), count = 0;
        if (n < 0)
            continue;
        for (int i = 0; i < n; i++)
            if (rows[i] >= 0 && rows[i] < catalog->count) {
                tracks[count] = catalog->tracks[rows[i]];
                tracks[count++].enabled = 1;
            }
        const ml_playlist *before = music_library_list(j->library, j->folder[k]);
        if (!before || (before->count == count && before->shuffle == shared &&
                        (!count || !memcmp(before->tracks, tracks, (size_t)count * sizeof *tracks))))
            continue;
        if (!music_library_replace(j->library, j->folder[k], tracks, count, shared))
            continue;
        if (k == 0)
            music_library_assign(j->library, ML_MENU, count ? j->folder[0] : -1);
        else
            music_library_assign(j->library, ML_REPLAY,
                                 count ? j->folder[1] : music_library_assignment(j->library, ML_RACE));
        music_library_enable(j->library, 1);
    }
    free(tracks);
}
static void mlj_audio(int16_t *out, int frames, void *user)
{
    ml_jukebox *j = user;
    if (!SDL_AtomicGet(&j->playing) ||
        !music_library_render(j->library, ML_MENU, out, frames, SDL_AtomicGet(&j->volume),
                              SDL_AtomicGet(&j->paused)))
        memset(out, 0, (size_t)frames * 4);
}
/* The page's sound.startStreamEx(group, volume, sequence) names a Library row; that
   row is played from its file or from the disc. Stop/Exit clear the group. */
static void mlj_audio_tick(ml_jukebox *j, int allowed)
{
    if (!j->page)
        return;
    int volume = 120, sequence = -1, repeat = 0;
    unsigned serial = 0;
    const char *group = menu_pcm_request(j->page, &volume);
    menu_pcm_parameters(j->page, &sequence, &repeat, &serial);
    if (serial != j->serial) {
        j->serial = serial;
        const ml_playlist *p = music_library_list(j->library, j->catalog);
        int playing = group && *group && p && sequence >= 0 && sequence < p->count;
        music_library_preview(j->library, playing ? j->catalog : -1, playing ? sequence : -1);
        SDL_AtomicSet(&j->playing, playing);
    }
    if ((!allowed || (group && !*group)) && SDL_AtomicGet(&j->playing)) {
        music_library_preview(j->library, -1, -1);
        SDL_AtomicSet(&j->playing, 0);
    }
    SDL_AtomicSet(&j->paused, menu_pcm_paused(j->page));
    SDL_AtomicSet(&j->volume, volume);
    /* Installed once: each install restarts the stream and reads its preroll
       from the song again, which ran it ahead (played fast) when done per frame. */
    if (SDL_AtomicGet(&j->playing)) {
        if (!plat_audio_music_owned(j))
            plat_audio_set_music(mlj_audio, j);
    }
    else
        plat_audio_release_music(j);
    menu_pcm_state(j->page,
                   SDL_AtomicGet(&j->playing) && music_library_preview_playing(j->library));
}
/* Called by both PCM update paths; nonzero means the Jukebox owns this page's stream. */
static void mlj_file_buttons(menu *m);
void mlj_tip_refresh(menu *m);
static int mlj_buttons_done;
static int app_jukebox_pcm(menu *m, int allowed)
{
    if (!mlj_buttons_done && m && menu_jukebox_page_widget(m)) {
        mlj_file_buttons(m);
        mlj_buttons_done = 1;
    }
    if (m && menu_jukebox_page_widget(m)) mlj_tip_refresh(m);
    if (!m || m != app_jukebox.page)
        return 0;
    mlj_audio_tick(&app_jukebox, allowed);
    return 1;
}
static int mlj_bind(ml_jukebox *j, menu *m, gt4_vol *vol)
{
    const ml_playlist *catalog = music_library_list(j->library, j->catalog);
    menu_music_entry *entries = calloc(ML_TRACKS, sizeof *entries);
    char(*information)[3][128] = calloc(ML_TRACKS, sizeof *information);
    if (!entries || !information) {
        free(entries);
        free(information);
        return 0;
    }
    int ids[2] = {j->race, j->slide}, shuffle[2] = {0, 0}, groups = mlj_groups_on();
    for (int i = 0; i < catalog->count; i++) {
        entries[i].title = catalog->tracks[i].title;
        entries[i].artist = "Unknown Artist";
        entries[i].genre = "";
        unsigned sequence;
        if (strncmp(catalog->tracks[i].path, "gt4:", 4)) {
            /* a file's name as its title, without the extension */
            mlj_clean_title(catalog->tracks[i].title, information[i][0],
                            sizeof information[i][0]);
            char *dot = strrchr(information[i][0], '.');
            if (dot && dot != information[i][0])
                *dot = 0;
            entries[i].title = information[i][0];
            if (groups) {
                char group[ML_PATH];
                mlj_group(catalog->tracks[i].path, group, sizeof group);
                snprintf(information[i][1], sizeof information[i][1], "%s", group);
                if (*group)
                    entries[i].artist = information[i][1];
            }
        } else if (sscanf(catalog->tracks[i].path, "gt4:%u", &sequence) == 1 &&
                   gt4_pcm_music_info(vol, sequence, information[i])) {
            entries[i].title = information[i][0];
            entries[i].artist = information[i][1];
            entries[i].genre = information[i][2];
        }
        for (int k = 0; k < 2; k++) {
            const ml_playlist *list = music_library_list(j->library, ids[k]);
            int row = mlj_find(list, catalog->tracks[i].path);
            entries[i].order[k] = row >= 0 && list->tracks[row].enabled ? row : -1;
            shuffle[k] = list ? list->shuffle : 0;
        }
    }
    if (app_video_settings &&
        moon_options_int(&app_video_settings->store, "audio.jukebox_empty_Race", 0, 0, 1))
        for (int i = 0; i < catalog->count; i++) entries[i].order[0] = -1;
    if (app_video_settings &&
        moon_options_int(&app_video_settings->store, "audio.jukebox_empty_Slideshow", 0, 0, 1))
        for (int i = 0; i < catalog->count; i++) entries[i].order[1] = -1;
    int listed[2] = {0, 0};
    for (int i = 0; i < catalog->count; i++)
        for (int k = 0; k < 2; k++)
            listed[k] += entries[i].order[k] >= 0;
    printf("jukebox: race list %d (%d songs), slideshow list %d (%d songs)\n", ids[0], listed[0],
           ids[1], listed[1]);
    int bound = menu_jukebox_catalog(m, entries, catalog->count, shuffle);
    free(information);
    free(entries);
    if (!bound)
        return 0;
    for (int i = 0; i < catalog->count; i++)
        mlj_shown[i] = i;
    if (groups) {
        /* the disc's songs as the disc lists them, then each folder's */
        int n = 0, locals;
        for (int i = 0; i < catalog->count; i++)
            if (!strncmp(catalog->tracks[i].path, "gt4:", 4))
                mlj_shown[n++] = i;
        locals = n;
        for (int i = 0; i < catalog->count; i++)
            if (strncmp(catalog->tracks[i].path, "gt4:", 4))
                mlj_shown[n++] = i;
        mlj_sorting = catalog;
        qsort(mlj_shown + locals, (size_t)(n - locals), sizeof *mlj_shown, mlj_group_order);
        menu_jukebox_library_order(m, mlj_shown, n);
    }
    if (j->folder[0] >= 0) {
        static int rows[2][ML_TRACKS];
        int counts[2] = {0, 0};
        const int *const ids[2] = {rows[0], rows[1]};
        for (int k = 0; k < 2; k++) {
            const ml_playlist *list = music_library_list(j->library, j->folder[k]);
            for (int i = 0; list && i < list->count && counts[k] < ML_TRACKS; i++) {
                int row = mlj_find(catalog, list->tracks[i].path);
                if (row >= 0)
                    rows[k][counts[k]++] = row;
            }
        }
        if (app_video_settings) {
            if (moon_options_int(&app_video_settings->store, "audio.jukebox_empty_Menu", 0, 0, 1))
                counts[0] = 0;
            if (moon_options_int(&app_video_settings->store, "audio.jukebox_empty_Replay", 0, 0, 1))
                counts[1] = 0;
        }
        menu_jukebox_folders(m, ids, counts);
        printf("jukebox: folders menu=%d replay=%d\n", counts[0], counts[1]);
    }
    j->page = m;
    j->serial = 0;
    return 1;
}
/* One folder next to the active options file, on every platform. No editor
   or file picker: rescan on Jukebox entry and keep the original Library UI. */
static void mlj_scan(ml_jukebox *j)
{
    char folder[ML_PATH];
    if (!app_music_folder(app_video_settings->store.path, folder, sizeof folder))
        return;
    int scanned = 0, before = music_library_list(j->library, j->catalog)->count;
    ml_add_folder(j->library, j->catalog, folder, 0, &scanned);
    snprintf(mlj_music_root, sizeof mlj_music_root, "%s", folder);
    int added = music_library_list(j->library, j->catalog)->count - before;
    int catalog = music_library_list(j->library, j->catalog)->count;
    printf("jukebox: Music folder=%s added=%d catalog=%d\n", folder, added, catalog);
    mlj_log("jukebox: Music folder=%s added=%d catalog=%d", folder, added, catalog);
}
/* GT Mode -> Jukebox (or a direct slide start): before the page is entered. With no
   local song the page stays exactly the disc's, on its PS2 playlist blobs. */
static const char *mlj_cell_text(ui_widget *w, char *buf, size_t size)
{
    if (!w) return NULL;
    if (w->text && w->type && !strcmp(w->type, "TextFace")) {
        snprintf(buf, size, "%s", w->text);
        return buf;
    }
    for (int i = 0; i < w->nkids; i++) {
        const char *t = mlj_cell_text(w->kids[i], buf, size);
        if (t) return t;
    }
    return NULL;
}
static int mlj_track_row(menu *m, int *widget_row)
{
    ui_widget *root = menu_jukebox_page_widget(m);
    ui_widget *list = root ? ui_widget_child(ui_widget_child(root, "List"), "list_box") : NULL;
    const ml_playlist *cat = music_library_list(app_jukebox.library, app_jukebox.catalog);
    int at = list ? list->focus_index : -1;
    if (widget_row) *widget_row = at;
    if (!list || !cat || at < 0 || at >= list->nkids) return -1;
    /* Resolve the focused row by what it SAYS, not by counting group headers.
       The rows print "<title> / <group>" and truncate the tail, so the row text
       is a prefix of the track's base name; take the longest track whose base
       name starts with it. Measured 2026-11-23: header counting drifted by rows
       and used to point at a gt4: track. */
    char raw[256];
    const char *text = mlj_cell_text(list->kids[at], raw, sizeof raw);
    if (!text) return -1;
    size_t n = strlen(text);
    const char *sep = strstr(text, " / ");
    if (sep) n = (size_t)(sep - text);
    while (n && (text[n - 1] == '.' || (unsigned char)text[n - 1] == 0x85)) n--;
    int best = -1;
    size_t bestn = 0;
    for (int j = 0; n && j < cat->count; j++) {
        char title[200];
        snprintf(title, sizeof title, "%s", cat->tracks[j].title);
        char *dot = strrchr(title, '.');
        if (dot && dot != title) *dot = 0;
        if (strncmp(title, text, n)) continue;
        if (strlen(title) == n) { best = j; break; } /* exact beats a longer sibling */
        if (n > bestn) { bestn = n; best = j; }
    }
    if (getenv("MLJ_DUMP")) {
        const char *p = best >= 0 ? cat->tracks[best].path : "(none)";
        fprintf(stderr, "jukebox map: row=%d -> %s  [text=%s]\n", at, p, text);
    }
    return best;
}
static const char *mlj_shown_tip(const ui_widget *w);
static const ml_track *mlj_footer_track(menu *m, ui_widget *root)
{
    const ml_playlist *cat = music_library_list(app_jukebox.library, app_jukebox.catalog);
    if (!cat || !root) return NULL;
    ui_widget *tip = ui_widget_child(root, "Tooltip");
    const char *text = NULL;
    for (int i = 0; tip && i < tip->nkids; i++)
        if (tip->kids[i] && tip->kids[i]->text) { text = tip->kids[i]->text; break; }
    if (!text) text = mlj_shown_tip(root);
    if (!text || !*text) return NULL;
    const char *start = strstr(text, "Song Details:");
    start = start ? start + 13 : text;
    while (*start == ' ') start++;
    const char *sep = strstr(start, " / ");
    if (!sep) return NULL;
    char title[160], group[ML_PATH];
    size_t n = (size_t)(sep - start);
    if (n >= sizeof title) n = sizeof title - 1;
    memcpy(title, start, n); title[n] = 0;
    const char *g = sep + 3;
    const char *end = strstr(g, " /");
    n = end ? (size_t)(end - g) : strlen(g);
    if (n >= sizeof group) n = sizeof group - 1;
    memcpy(group, g, n); group[n] = 0;
    for (int i = 0; i < cat->count; i++) {
        if (!strncmp(cat->tracks[i].path, "gt4:", 4)) continue;
        char display[160], grp[ML_PATH];
        snprintf(display, sizeof display, "%s", cat->tracks[i].title);
        char *dot = strrchr(display, '.');
        if (dot && dot != display) *dot = 0;
        mlj_group(cat->tracks[i].path, grp, sizeof grp);
        size_t tl = strlen(title), gl = strlen(group);
        int tmatch = !strcmp(display, title) ||
                     (tl >= 12 && !strncmp(display, title, tl));
        int gmatch = !strcmp(grp, group) || (gl >= 6 && !strncmp(grp, group, gl));
        if (tmatch && gmatch && access(cat->tracks[i].path, F_OK) == 0)
            return &cat->tracks[i];
    }
    return NULL;
}
static const ml_track *mlj_selected(menu *m)
{
    if (!m || m != app_jukebox.page || menu_active_root(m) < 0) return NULL;
    ui_widget *root = ui_root_widget(menu_ui(m), menu_active_root(m));
    const ml_track *footer = mlj_footer_track(m, root);
    if (footer) return footer;
    ui_widget *list = ui_widget_child(ui_widget_child(root, "List"), "list_box");
    const ml_playlist *cat = music_library_list(app_jukebox.library, app_jukebox.catalog);
    int row = list ? list->focus_index : -1;
    if (!cat || row < 0) return NULL;
    int idx = mlj_track_row(m, &row);
    if (idx < 0 || idx >= cat->count) return NULL;
    if (strncmp(cat->tracks[idx].path, "gt4:", 4) &&
        access(cat->tracks[idx].path, F_OK) != 0)
        return NULL;
    return &cat->tracks[idx];
}
static void mlj_refresh(menu *m, int row);
static int mlj_delete_activate(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)vm; (void)self; (void)argc; (void)argv;
    menu *m = data;
    const ml_track *t = mlj_selected(m);
    *ret = ah_int(2);
    if (!t) { menu_confirm(m, "Delete", "This is a folder, not a song."); return 0; }
    if (!strncmp(t->path, "gt4:", 4)) {
        menu_confirm(m, "Delete", "This song is part of the game disc.");
        return 0;
    }
    char msg[512];
    snprintf(msg, sizeof msg, "Delete %s?", t->title[0] ? t->title : "this song");
    if (menu_confirm(m, "Delete", msg)) {
        if (remove(t->path) == 0) {
            mlj_log("jukebox: deleted %s", t->path);
            printf("jukebox: deleted %s\n", t->path);
            ui_widget *root = menu_jukebox_page_widget(m);
            ui_widget *list = root ? ui_widget_child(ui_widget_child(root, "List"), "list_box") : NULL;
            int row = list ? list->focus_index : 0, idx = mlj_track_row(m, &row);
            if (idx >= 0) {
                music_library_remove(app_jukebox.library, app_jukebox.catalog, idx);
                menu_jukebox_drop_row(m, idx);
            }
            mlj_refresh(m, row > 0 ? row - 1 : 0);
        } else
            mlj_log("jukebox: could not delete %s", t->path);
    }
    return 0;
}
static struct {
    int active, row, idx;
    menu *owner;
    char name[256], old[1200], ext[32], warning[96];
} mlj_rename;
static int mlj_rename_root = -1;
static void mlj_rename_native_setup(void)
{
    if (!dialog_ui) return;
    if (mlj_rename_root < 0)
        for (int i = 0; i < ui_root_count(dialog_ui); i++)
            if (!strcmp(ui_root_name(dialog_ui, i), "ConfirmDialog"))
                mlj_rename_root = i;
    if (mlj_rename_root < 0) return;
    ui_widget *page = ui_root_widget(dialog_ui, mlj_rename_root);
    ui_widget *pane = ui_widget_child(page, "Dialog");
    ui_widget *title = ui_widget_child(pane, "Title"), *body = ui_widget_child(pane, "Message"),
              *buttons = ui_widget_child(pane, "Buttons");
    ui_widget *single = ui_widget_child(buttons, "OK"), *pair = ui_widget_child(buttons, "YesNo");
    ui_widget *positive = ui_widget_child(pair, "Yes"), *negative = ui_widget_child(pair, "No");
    if (!page || !pane || !title || !body || !buttons || !single || !pair || !positive ||
        !negative) return;
    /* normal (non-warning) chrome, the way present_dialog styles every dialog —
       without this the panel inherits whatever warning state the last dialog left */
    moon_dialog_style(dialog_ui, page, 1, "Rename song");
    single->visible = 0; pair->visible = 1;
    free(ui_widget_child(positive, "label")->text);
    ui_widget_child(positive, "label")->text = strdup("OK");
    free(ui_widget_child(negative, "label")->text);
    ui_widget_child(negative, "label")->text = strdup("Cancel");
    positive->focused = 1; negative->focused = 0;
    buttons->y = body->y + body->h;
    pane->h = buttons->y + buttons->h;
    pane->y = (480 - pane->h) * .5f;
    ui_widget *bg = ui_widget_child(pane, "bg");
    if (bg) bg->h = pane->h;
    page->opacity = 1;
    page->visible = 1;
}
static void mlj_refresh(menu *m, int row)
{
    const char *disc = studio_disc ? studio_disc : presentation_disc;
    const char *err = NULL;
    gt4_vol *v = disc && *disc ? gt4_vol_open(disc, &err) : NULL;
    if (v) {
        mlj_bind(&app_jukebox, m, v);
        gt4_vol_close(v);
    }
    menu_jukebox_refresh(m);
    ui_widget *root = menu_jukebox_page_widget(m);
    ui_widget *list = root ? ui_widget_child(ui_widget_child(root, "List"), "list_box") : NULL;
    if (list) list->focus_index = row;
    mlj_file_buttons(m);
}
/* The Library rows are guest composites whose visible text is a cached TextFace
   inside the row, so writing the catalog/guest array is not enough: the pixels
   only follow if this cell is rewritten. Measured 2026-11-23: title cell is
   kids[3] named "title", text "Song / Group". */
static void mlj_row_text(menu *m, int row, const char *title)
{
    ui_widget *root = m ? menu_jukebox_page_widget(m) : NULL;
    ui_widget *list = root ? ui_widget_child(ui_widget_child(root, "List"), "list_box") : NULL;
    if (!list || !title || row < 0 || row >= list->nkids)
        return;
    ui_widget *r = list->kids[row];
    for (int i = 0; r && i < r->nkids; i++) {
        ui_widget *c = r->kids[i];
        if (!c || !c->text || !c->type || strcmp(c->type, "TextFace"))
            continue;
        char buf[256];
        const char *slash = strstr(c->text, " / ");
        if (slash)
            snprintf(buf, sizeof buf, "%s%s", title, slash);
        else
            snprintf(buf, sizeof buf, "%s", title);
        free(c->text);
        c->text = strdup(buf);
        fprintf(stderr, "jukebox: row %d retitled to %s\n", row, buf);
        return;
    }
}
static void mlj_rename_begin(menu *m)
{
    const ml_track *t = mlj_selected(m);
    if (!t) return;
    if (!strncmp(t->path, "gt4:", 4)) {
        menu_confirm(m, "Rename", "This song is part of the game disc.");
        return;
    }
    mlj_rename.owner = m;
    mlj_rename.row = mlj_shown && 1 ? 0 : 0;
    {
        ui_widget *root = menu_jukebox_page_widget(m);
        ui_widget *list = root ? ui_widget_child(ui_widget_child(root, "List"), "list_box") : NULL;
        mlj_rename.row = list ? list->focus_index : 0;
        mlj_rename.idx = mlj_track_row(m, &mlj_rename.row);
    }
    snprintf(mlj_rename.old, sizeof mlj_rename.old, "%s", t->path);
    const char *base = strrchr(t->path, '/');
    base = base ? base + 1 : t->path;
    const char *dot = strrchr(base, '.');
    size_t n = dot ? (size_t)(dot - base) : strlen(base);
    if (n >= sizeof mlj_rename.name) n = sizeof mlj_rename.name - 1;
    memcpy(mlj_rename.name, base, n);
    mlj_rename.name[n] = 0;
    snprintf(mlj_rename.ext, sizeof mlj_rename.ext, "%s", dot ? dot : "");
    mlj_rename.warning[0] = 0;
    mlj_rename.active = 1;
    mlj_rename_native_setup();
    SDL_StartTextInput();
}
static int mlj_rename_activate(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)vm; (void)self; (void)argc; (void)argv;
    *ret = ah_int(2);
    mlj_rename_begin(data);
    return 0;
}
static int mlj_rename_valid(const char *s)
{
    if (!*s) return 0;
    for (; *s; s++)
        if ((unsigned char)*s < 0x20 || *s == 0x7f || strchr("/\\:*?\"<>|", *s))
            return 0;
    return 1;
}
static void mlj_rename_commit(void)
{
    if (!mlj_rename_valid(mlj_rename.name)) {
        snprintf(mlj_rename.warning, sizeof mlj_rename.warning, "Invalid file name.");
        return;
    }
    char path[1400];
    snprintf(path, sizeof path, "%s", mlj_rename.old);
    char *slash = strrchr(path, '/');
    if (slash) slash[1] = 0; else path[0] = 0;
    char next[1500];
    snprintf(next, sizeof next, "%s%s%s", path, mlj_rename.name, mlj_rename.ext);
    if (!strcmp(next, mlj_rename.old)) {
        mlj_rename.active = 0;
        SDL_StopTextInput();
        return;
    }
    if (rename(mlj_rename.old, next) == 0) {
        fprintf(stderr, "jukebox: renamed %s -> %s\n", mlj_rename.old, next);
        mlj_log("jukebox: renamed %s -> %s", mlj_rename.old, next);
        if (mlj_rename.idx >= 0) {
            music_library_remove(app_jukebox.library, app_jukebox.catalog, mlj_rename.idx);
            music_library_add(app_jukebox.library, app_jukebox.catalog, next, mlj_rename.name);
        }
        if (mlj_rename.owner) {
            mlj_refresh(mlj_rename.owner, mlj_rename.row);
            menu_jukebox_retitle(mlj_rename.owner, mlj_rename.idx, mlj_rename.name);
            mlj_row_text(mlj_rename.owner, mlj_rename.row, mlj_rename.name);
        }
        mlj_rename.active = 0;
        SDL_StopTextInput();
    } else {
        snprintf(mlj_rename.warning, sizeof mlj_rename.warning, "Rename failed.");
        fprintf(stderr, "jukebox: rename failed %s -> %s\n", mlj_rename.old, next);
    }
}
int mlj_rename_active(void) { return mlj_rename.active; }
int mlj_rename_event(plat_key_event *ev)
{
    if (!mlj_rename.active) return 0;
    if (ev->text[0]) {
        size_t n = strlen(mlj_rename.name), add = strlen(ev->text);
        if (n + add < sizeof mlj_rename.name) memcpy(mlj_rename.name + n, ev->text, add + 1);
        mlj_rename.warning[0] = 0;
        return 1;
    }
    if (ev->key == SDLK_BACKSPACE) {
        size_t n = strlen(mlj_rename.name);
        if (n) mlj_rename.name[n - 1] = 0;
        mlj_rename.warning[0] = 0;
        return 1;
    }
    if (ev->key == SDLK_ESCAPE) {
        mlj_rename.active = 0;
        SDL_StopTextInput();
        return 1;
    }
    if (ev->key == SDLK_RETURN || ev->key == SDLK_KP_ENTER) {
        mlj_rename_commit();
        return 1;
    }
    return 1;
}
int mlj_rename_sdl_event(const SDL_Event *ev)
{
    if (!mlj_rename.active || !ev) return 0;
    plat_key_event k;
    memset(&k, 0, sizeof k);
    if (ev->type == SDL_TEXTINPUT) snprintf(k.text, sizeof k.text, "%s", ev->text.text);
    else if (ev->type == SDL_KEYDOWN) k.key = ev->key.keysym.sym;
    else return 0;
    return mlj_rename_event(&k);
}
void mlj_rename_draw(void)
{
    if (!mlj_rename.active || !dialog_ui || mlj_rename_root < 0) return;
    ui_widget *page = ui_root_widget(dialog_ui, mlj_rename_root);
    ui_widget *pane = ui_widget_child(page, "Dialog");
    ui_widget *body = ui_widget_child(pane, "Message");
    ui_widget *face = ui_widget_child(body, "message");
    if (face) {
        char text[512];
        if (mlj_rename.warning[0])
            snprintf(text, sizeof text, "%s\n%s", mlj_rename.name, mlj_rename.warning);
        else
            snprintf(text, sizeof text, "%s", mlj_rename.name);
        free(face->text); face->text = strdup(text);
        face->w = 300; face->h = 50;
        face->scale_x = face->scale_y = 1;
        ui_text_wrap(dialog_ui, face, 300);
        ui_text_adjust_scale(dialog_ui, face);
        float size[2];
        ui_text_size(dialog_ui, face, size);
        body->h = fmaxf(82, size[1] + 32);
        ui_widget *buttons = ui_widget_child(pane, "Buttons");
        buttons->y = body->y + body->h;
        pane->h = buttons->y + buttons->h;
        pane->y = (480 - pane->h) * .5f;
    }
    ui_draw_root(dialog_ui, mlj_rename_root, NULL, NULL);
}


void mlj_jukebox_rename_now(void)
{
    if (app_jukebox.page) mlj_rename_begin(app_jukebox.page);
}
int mlj_jukebox_page_active(void) { return app_jukebox.page != NULL; }
void mlj_tip_refresh(menu *m)
{
    ui_widget *root = menu_jukebox_page_widget(m);
    ui_widget *hint = root ? ui_widget_child(root, "MoonHint") : NULL;
    if (!hint) return;
    const ml_track *t = mlj_selected(m);
    hint->visible = t && strncmp(t->path, "gt4:", 4) && !mlj_rename.active;
}
static ui_widget *mlj_text_widget(ui_widget *w, const char *needle)
{
    if (!w) return NULL;
    if (w->text && strstr(w->text, needle)) return w;
    for (int i = 0; i < w->nkids; i++) {
        ui_widget *r = mlj_text_widget(w->kids[i], needle);
        if (r) return r;
    }
    return NULL;
}
static void mlj_empty_activate(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)vm; (void)self; (void)argc; (void)argv;
    *ret = ah_int(2);
    fprintf(stderr, "jukebox Empty activate\n"); fflush(stderr);
    menu *m = data;
    char name[64];
    int id = mlj_category_id(m, name, sizeof name);
    if (id < 0) return;
    const ml_playlist *list = music_library_list(app_jukebox.library, id);
    if (!list) return;
    ui_widget *root = menu_jukebox_page_widget(m);
    const char *foot = root ? mlj_find_text(root, "No. of songs:") : NULL;
    int shown = foot ? atoi(strstr(foot, "No. of songs:") + 13) : (int)list->count;
    char msg[256];
    snprintf(msg, sizeof msg, "Remove all %d songs from %s?", shown, name);
    if (menu_confirm(m, "Empty category", msg)) {
        char key[64];
        snprintf(key, sizeof key, "audio.jukebox_empty_%s", name);
        if (app_video_settings) {
            moon_options_set(&app_video_settings->store, key, "1");
            moon_options_save(&app_video_settings->store);
        }
        int li = !strcmp(name, "Race") ? 1 : !strcmp(name, "Slideshow") ? 2
                 : !strcmp(name, "Menu") ? 3 : 4;
        menu_jukebox_clear_list(m, li);
        menu_jukebox_set_mode(m, li);
        ui_widget *fr = menu_jukebox_page_widget(m);
        ui_widget *tw = mlj_text_widget(fr, "No. of songs:");
        if (tw) {
            char buf[256];
            snprintf(buf, sizeof buf, "%s", tw->text);
            char *colon = strstr(buf, "No. of songs:");
            if (colon) snprintf(colon + 13, sizeof buf - (size_t)(colon + 13 - buf), "0");
            free(tw->text); tw->text = strdup(buf);
        }
        printf("jukebox: emptied category %s (marker %s=1)\n", name, key);
        fflush(stdout);
        mlj_log("jukebox: emptied category %s", name);
    }
}
static int mlj_hidden_has(const char *cat)
{
    if (!app_video_settings || !cat || !*cat) return 0;
    const char *h = moon_options_get(&app_video_settings->store, "audio.jukebox_hidden", "");
    size_t n = strlen(cat);
    for (const char *p = h; p && *p;) {
        const char *e = strchr(p, ',');
        size_t k = e ? (size_t)(e - p) : strlen(p);
        if (k == n && !strncmp(p, cat, n)) return 1;
        p = e ? e + 1 : NULL;
    }
    return 0;
}
static void mlj_hidden_add(const char *cat)
{
    if (!app_video_settings || !cat || !*cat || mlj_hidden_has(cat)) return;
    char h[512];
    snprintf(h, sizeof h, "%s%s%s", moon_options_get(&app_video_settings->store,
                                                    "audio.jukebox_hidden", ""),
             *moon_options_get(&app_video_settings->store, "audio.jukebox_hidden", "") ? "," : "",
             cat);
    moon_options_set(&app_video_settings->store, "audio.jukebox_hidden", h);
    moon_options_save(&app_video_settings->store);
}
static void mlj_apply_hidden(menu *m)
{
    const ml_playlist *cat = music_library_list(app_jukebox.library, app_jukebox.catalog);
    if (!cat) return;
    for (int i = cat->count - 1; i >= 0; i--) {
        const char *g = menu_jukebox_row_group(m, i);
        if (g && mlj_hidden_has(g)) menu_jukebox_drop_row(m, i);
    }
}
void mlj_jukebox_empty_category(void)
{
    ah_val ret;
    fprintf(stderr, "jukebox Ctrl+E fired\n"); fflush(stderr);
    mlj_empty_activate(NULL, NULL, app_jukebox.page, 0, NULL, &ret);
}
void mlj_jukebox_empty_click(float u, float v)
{
    if (u < 500 || u > 534 || v < 415 || v > 439) return;
    char name[64];
    if (mlj_category_id(app_jukebox.page, name, sizeof name) < 0) return;
    fprintf(stderr, "jukebox Empty click %.1f,%.1f -> %s\n", u, v, name); fflush(stderr);
    mlj_jukebox_empty_category();
}
void mlj_jukebox_remove_category(void)
{
    menu *m = app_jukebox.page;
    fprintf(stderr, "jukebox Ctrl+H: page=%p\n", (void *)m);
    if (!m || menu_active_root(m) < 0) { fprintf(stderr, "jukebox Ctrl+H: no page\n"); fflush(stderr); return; }
    const ml_track *t = mlj_selected(m);
    if (!t) { fprintf(stderr, "jukebox Ctrl+H: no track\n"); fflush(stderr); return; }
    const ml_playlist *cat = music_library_list(app_jukebox.library, app_jukebox.catalog);
    if (!cat) { fprintf(stderr, "jukebox Ctrl+H: no list\n"); fflush(stderr); return; }
    int idx = -1;
    for (int i = 0; i < cat->count; i++)
        if (&cat->tracks[i] == t) { idx = i; break; }
    char group[ML_PATH] = "";
    if (strncmp(t->path, "gt4:", 4))
        mlj_group(t->path, group, sizeof group);
    else {
        const char *g = idx >= 0 ? menu_jukebox_row_group(m, idx) : NULL;
        if (g) snprintf(group, sizeof group, "%s", g);
    }
    fprintf(stderr, "jukebox Ctrl+H: track=%s path=%s idx=%d group=%s\n",
            t->title, t->path, idx, group);
    if (!group[0]) { fprintf(stderr, "jukebox Ctrl+H: no group\n"); fflush(stderr); return; }
    int count = 0;
    for (int i = 0; i < cat->count; i++) {
        const char *cg = menu_jukebox_row_group(m, i);
        char local[ML_PATH] = "";
        if (!cg && strncmp(cat->tracks[i].path, "gt4:", 4)) {
            mlj_group(cat->tracks[i].path, local, sizeof local);
            cg = local;
        }
        if (cg && !strcmp(cg, group)) count++;
    }
    fprintf(stderr, "jukebox Ctrl+H: count=%d\n", count); fflush(stderr);
    char msg[256];
    snprintf(msg, sizeof msg, "Remove category \"%s\" (%d songs)?", group, count);
    int yes = menu_confirm(m, "Remove category", msg);
    fprintf(stderr, "jukebox Ctrl+H: %s\n", yes ? "confirm shown/accepted" : "cancelled"); fflush(stderr);
    if (!yes) return;
    for (int i = cat->count - 1; i >= 0; i--) {
        const char *cg = menu_jukebox_row_group(m, i);
        char local[ML_PATH] = "";
        if (!cg && strncmp(cat->tracks[i].path, "gt4:", 4)) {
            mlj_group(cat->tracks[i].path, local, sizeof local);
            cg = local;
        }
        if (cg && !strcmp(cg, group)) menu_jukebox_drop_row(m, i);
    }
    mlj_hidden_add(group);
    printf("jukebox: removed category %s (%d songs)\n", group, count);
    mlj_log("jukebox: removed category %s (%d songs)", group, count);
}

int mlj_jukebox_hotkey(int sym)
{
    menu *m = app_jukebox.page;
    if (!m) return 0;
    ah_val ret;
    if (sym == 0xffff || sym == 0xff08) {
        mlj_delete_activate(NULL, NULL, m, 0, NULL, &ret);
        return 1;
    }
    return 0;
}
static int mlj_jukebox_key(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)vm; (void)self; (void)argc; (void)argv;
    menu *m = data;
    ah_val key;
    *ret = ah_int(0);
    if (argc < 2 || !ah_get_member(vm, argv[1].u.o, "keysym", &key)) return 0;
    int sym = (int)key.u.i;
    if (sym == 0xffff || sym == 0xff08) { /* Delete, Backspace */
        mlj_delete_activate(vm, self, m, 0, NULL, ret);
        *ret = ah_int(2);
    }
    return 0;
}
static void mlj_file_buttons(menu *m)
{
    ui_project *ui = menu_ui(m);
    ui_widget *root = menu_jukebox_page_widget(m);
    ui_widget *lib = root ? ui_widget_child(root, "Library") : NULL;
    ui_widget *liblabel = lib ? ui_widget_child(lib, "label") : NULL;
    const char *old[5] = {"MoonDelete", "MoonRename", "MoonDeleteLabel", "MoonRenameLabel", "MoonHint"};
    for (int i = 0; i < 5; i++) {
        ui_widget *w = root ? ui_widget_child(root, old[i]) : NULL;
        if (w) ui_widget_detach(w);
    }
    if (root && liblabel) {
        ui_widget *h = ui_widget_copy(ui, liblabel);
        h->name = "MoonHint";
        free(h->text);
        h->text = strdup("Del: delete   Ctrl+R: rename   Ctrl+H: remove category   Ctrl+E: empty this category");
        h->x = 250.f;
        h->y = 418.f;
        h->w = 360.f;
        h->h = 22.f;
        h->has_geometry = 1;
        h->scale_x = h->scale_y = .34f;
        h->visible = 0;
        ui_widget_append(root, h);
    }
    if (root) {
        menu_native_method(m, root, "onKeyPress", mlj_jukebox_key, m);
        ui_widget *klist = ui_widget_child(ui_widget_child(root, "List"), "list_box");
        if (klist) menu_native_method(m, klist, "onKeyPress", mlj_jukebox_key, m);
    }
}
static void mlj_prune(ml_jukebox *j)
{
    if (!j || !j->library || j->catalog < 0) return;
    int again = 1;
    while (again) {
        again = 0;
        const ml_playlist *cat = music_library_list(j->library, j->catalog);
        for (int i = 0; cat && i < cat->count; i++) {
            const char *path = cat->tracks[i].path;
            if (strncmp(path, "gt4:", 4) && access(path, F_OK) != 0) {
                music_library_remove(j->library, j->catalog, i);
                again = 1;
                break;
            }
        }
    }
}
static void app_jukebox_attach(menu *m)
{
    if (!m || app_jukebox.page == m)
        return;
    const char *options =
        app_video_settings && app_video_settings->store.path[0] ? app_video_settings->store.path
                                                                : "-";
    int custom = app_video_settings
                     ? moon_options_int(&app_video_settings->store, "audio.custom_music", 0, 0, 1)
                     : 0;
    mlj_log("jukebox: attach called options=%s custom_music=%d", options, custom);
    /* Custom Music off: the disc's own page, on its PS2 playlist blobs */
    if (!app_video_settings || !custom) {
        printf("jukebox: native disc-only entry, custom music off\n");
        mlj_log("jukebox: early return, custom music off (audio.custom_music=%d)", custom);
        return;
    }
    app_music_ensure(1);
    if (!app_music_library) {
        mlj_log("jukebox: early return, library could not be opened");
        return;
    }
    const char *disc = studio_disc ? studio_disc : presentation_disc, *error = NULL;
    gt4_vol *vol = disc ? gt4_vol_open(disc, &error) : NULL;
    if (!vol) {
        mlj_log("jukebox: early return, disc volume could not be opened (disc=%s)",
                disc ? disc : "-");
        return;
    }
    ml_jukebox j = {0};
    j.library = app_music_library;
    j.folder[0] = j.folder[1] = -1;
    menu_jukebox_folders(m, NULL, NULL);
    int local = 0;
    if (!mlj_seed(&j, vol))
        mlj_log("jukebox: seeding failed, continuing with the local Music folder");
    if (j.catalog < 0)
        j.catalog = music_library_catalog(j.library);
    if (j.catalog >= 0) {
        mlj_scan(&j);
        mlj_prune(&j);
        const ml_playlist *catalog = music_library_list(j.library, j.catalog);
        if (catalog)
            for (int i = 0; i < catalog->count && !local; i++)
                local = strncmp(catalog->tracks[i].path, "gt4:", 4) != 0;
    } else {
        mlj_log("jukebox: Music folder not scanned, catalog unavailable");
    }
    /* the folders keep their lists in the library, disc songs included */
    if (!local && j.folder[0] < 0)
        printf("jukebox: native disc-only entry\n");
    else if (mlj_bind(&j, m, vol)) {
        mlj_apply_hidden(m);
        app_jukebox.library = j.library;
        app_jukebox.page = j.page;
        app_jukebox.race = j.race;
        app_jukebox.slide = j.slide;
        app_jukebox.catalog = j.catalog;
        app_jukebox.folder[0] = j.folder[0];
        app_jukebox.folder[1] = j.folder[1];
        app_jukebox.serial = 0;
        SDL_AtomicSet(&app_jukebox.playing, 0);
        mlj_buttons_done = 0;
        printf("jukebox: original Library, master=%d folders=%d\n", music_library_enabled(j.library),
               j.folder[0] >= 0);
    }
    gt4_vol_close(vol);
}
/* Leaving the slide project (its Exit, or the game closing): Racing/Slideshow as
   the page left them, then the original stream path back. */
static void app_jukebox_leave(void)
{
    ml_jukebox *j = &app_jukebox;
    if (!j->page)
        return;
    music_library_preview(j->library, -1, -1);
    SDL_AtomicSet(&j->playing, 0);
    plat_audio_release_music(j);
    mlj_store(j);
    printf("custom music: rendered %u external frames, peak %d\n", music_library_played(j->library),
           music_library_peak(j->library));
    j->page = NULL;
}
/* custom_music_folders: the Menu and Replay folders on the same page. The
   help line after each mouse hover/click is printed as "jukebox: tip <name>=";
   shots <prefix>-library/-menu/-replay/-groups.png; then the authored Exit. */
static const char *mlj_shown_tip(const ui_widget *w)
{
    if (w && !strcmp(w->type, "ToolTipFace"))
        return w->text;
    for (int i = 0; w && i < w->nkids; i++) {
        const char *text = mlj_shown_tip(w->kids[i]);
        if (text)
            return text;
    }
    return NULL;
}
static void mlj_point(menu *m, ui_widget *w, int click)
{
    const float *r = w ? w->pointer_rect : NULL;
    if (!r)
        return;
    menu_pointer(m, 1, r[0] + r[2] / 2, r[1] + r[3] / 2, 0, 0);
    if (click) {
        menu_pointer(m, 1, r[0] + r[2] / 2, r[1] + r[3] / 2, 3, 0);
        menu_pointer(m, 1, r[0] + r[2] / 2, r[1] + r[3] / 2, 1, 0);
    }
}
static int mlj_folder_fixture(menu *m, const char *prefix)
{
    static int frame, step;
    if (step > 11)
        return 0;
    if (!m || m != app_jukebox.page || menu_active_root(m) < 0 ||
        strcmp(ui_root_name(menu_ui(m), menu_active_root(m)), "MusicRoot") ||
        ui_fade_remaining(menu_ui(m)) > 0)
        return 1;
    if (frame++ % 20 != 15)
        return 1;
    ui_widget *root = ui_root_widget(menu_ui(m), menu_active_root(m));
    ui_widget *list = ui_widget_child(ui_widget_child(root, "List"), "list_box");
    const ml_playlist *catalog = music_library_list(app_jukebox.library, app_jukebox.catalog);
    char path[1024];
    /* what the previous step pointed at or clicked */
    static const char *const tips[] = {NULL, NULL, "Append3", "Append4", NULL, NULL,
                                       NULL, "PlayList1", "PlayList3", "PlayList3-click",
                                       "PlayList4", NULL};
    static const char *const shots[] = {NULL, "library", NULL, NULL, NULL, NULL,
                                        NULL, NULL, NULL, "menu", "replay", "groups"};
    if (tips[step]) {
        const char *tip = mlj_shown_tip(root);
        printf("jukebox: tip %s=%s\n", tips[step], tip ? tip : "");
    }
    if (shots[step]) {
        snprintf(path, sizeof path, "%s-%s.png", prefix, shots[step]);
        screenshot(path);
    }
    switch (step) {
    case 0: { /* a local song selected in the Library */
        int row = -1;
        for (int i = 0; i < catalog->count && row < 0; i++)
            if (strstr(catalog->tracks[mlj_shown[i]].path, "Som S"))
                row = i;
        menu_native_focus(m, ui_widget_child(root, "Library"));
        menu_send_input(m, MENU_ACCEPT);
        menu_native_focus(m, list);
        for (int i = 0; i < row; i++)
            menu_send_input(m, MENU_DOWN);
        menu_send_input(m, MENU_ACCEPT);
        break;
    }
    case 1:
    case 2:
        mlj_point(m, ui_widget_child(root, step == 1 ? "Append3" : "Append4"), 0);
        break;
    case 3:
    case 4: /* added by the pad, as Append1 */
        menu_pointer(m, 0, 0, 0, 0, 0);
        menu_native_focus(m, ui_widget_child(root, step == 3 ? "Append3" : "Append4"));
        menu_send_input(m, MENU_ACCEPT);
        break;
    case 6:
        mlj_point(m, ui_widget_child(root, "PlayList1"), 0);
        break;
    case 7:
        mlj_point(m, ui_widget_child(root, "PlayList3"), 0);
        break;
    case 8:
        mlj_point(m, ui_widget_child(root, "PlayList3"), 1);
        break;
    case 9:
        mlj_point(m, ui_widget_child(root, "PlayList4"), 1);
        break;
    case 10:
        menu_pointer(m, 0, 0, 0, 0, 0);
        menu_native_focus(m, ui_widget_child(root, "Library"));
        menu_send_input(m, MENU_ACCEPT);
        menu_native_focus(m, list);
        for (int i = 0; i < catalog->count - 1; i++)
            menu_send_input(m, MENU_DOWN);
        break;
    case 11:
        printf("jukebox: folders fixture passed\n");
        menu_native_focus(m, ui_widget_child(root, "Exit"));
        menu_send_input(m, MENU_ACCEPT);
        break;
    }
    step++;
    return 1;
}
/* MOONSTUDIO_JUKEBOX_VIEW: the Library, Racing and Slideshow folders as the page shows
   them (<prefix>-library/-race/-slide.png), then Exit. */
static int mlj_view_fixture(menu *m, const char *prefix)
{
    static int frame, step;
    static const char *const folders[] = {"Library", "PlayList1", "PlayList2"},
                             *const names[] = {"library", "race", "slide"};
    if (step > 6)
        return 0;
    if (!m || m != app_jukebox.page || menu_active_root(m) < 0 ||
        strcmp(ui_root_name(menu_ui(m), menu_active_root(m)), "MusicRoot") ||
        ui_fade_remaining(menu_ui(m)) > 0)
        return 1;
    if (frame++ % 30 != 25)
        return 1;
    ui_widget *root = ui_root_widget(menu_ui(m), menu_active_root(m));
    char path[1024];
    if (step == 6) {
        printf("jukebox: view fixture done\n");
        menu_native_focus(m, ui_widget_child(root, "Exit"));
        menu_send_input(m, MENU_ACCEPT);
    } else if (step % 2 == 0) {
        menu_native_focus(m, ui_widget_child(root, folders[step / 2]));
        menu_send_input(m, MENU_ACCEPT);
    } else {
        snprintf(path, sizeof path, "%s-%s.png", prefix, names[step / 2]);
        screenshot(path);
    }
    step++;
    return 1;
}
/* custom_music_ui: drives the page the game opened, then leaves by its Exit.
   Nonzero while steps remain, so the caller holds its own screenshot. */
static int app_jukebox_fixture(menu *m)
{
    static int frame, step;
    const char *shot = getenv("MOONSTUDIO_MUSIC_SHOT");
    if (getenv("MOONSTUDIO_FOLDER_SHOT") && getenv("MOON_WINDOW_HIDDEN"))
        return mlj_folder_fixture(m, getenv("MOONSTUDIO_FOLDER_SHOT"));
    if (getenv("MOONSTUDIO_JUKEBOX_VIEW") && getenv("MOON_WINDOW_HIDDEN"))
        return mlj_view_fixture(m, getenv("MOONSTUDIO_JUKEBOX_VIEW"));
    if (!shot || !getenv("MOON_WINDOW_HIDDEN") || step > 12)
        return 0;
    if (!m || m != app_jukebox.page || menu_active_root(m) < 0 ||
        strcmp(ui_root_name(menu_ui(m), menu_active_root(m)), "MusicRoot") ||
        ui_fade_remaining(menu_ui(m)) > 0)
        return 1;
    if (frame++ % 20 != 15)
        return 1;
    ui_project *ui = menu_ui(m);
    ui_widget *root = ui_root_widget(ui, menu_active_root(m));
    ui_widget *list = ui_widget_child(ui_widget_child(root, "List"), "list_box");
    const ml_playlist *catalog = music_library_list(app_jukebox.library, app_jukebox.catalog);
    if (step == 0 || step == 6) {
        menu_native_focus(m, ui_widget_child(root, "Library"));
        menu_send_input(m, MENU_ACCEPT);
        menu_native_focus(m, list);
        int row = -1;
        for (int i = 0; i < catalog->count; i++)
            if (strstr(catalog->tracks[mlj_shown[i]].path, "Som S")) {
                row = i;
                break;
            }
        if (row < 0) {
            fprintf(stderr, "jukebox fixture: local song missing from the Library\n");
            step = 13;
            return 0;
        }
        for (int i = 0; i < row; i++)
            menu_send_input(m, MENU_DOWN);
        menu_send_input(m, MENU_ACCEPT);
    }
    if (step == 1 || step == 7) {
        menu_native_focus(m, ui_widget_child(root, "Append1"));
        menu_send_input(m, MENU_ACCEPT);
    }
    if (step == 2) {
        menu_native_focus(m, ui_widget_child(root, "Append2"));
        menu_send_input(m, MENU_ACCEPT);
    }
    if (step == 3 || step == 8) {
        menu_native_focus(m, ui_widget_child(root, "PlayList1"));
        menu_send_input(m, MENU_ACCEPT);
    }
    if (step == 4) {
        menu_native_focus(m, list);
        for (int i = 0; i < list->nkids - 1; i++)
            menu_send_input(m, MENU_DOWN);
        menu_send_input(m, MENU_ACCEPT);
    }
    if (step == 5) {
        menu_native_focus(m, ui_widget_child(root, "Trash"));
        menu_send_input(m, MENU_ACCEPT);
    }
    if (step == 9) {
        menu_native_focus(m, ui_widget_child(root, "Play"));
        menu_send_input(m, MENU_ACCEPT);
    }
    if (step == 10) {
        menu_native_focus(m, ui_widget_child(root, "PlayMode"));
        menu_send_input(m, MENU_ACCEPT);
    }
    if (step == 11) {
        menu_native_focus(m, ui_widget_child(root, "Library"));
        menu_send_input(m, MENU_ACCEPT);
        menu_native_focus(m, list);
        for (int i = 0; i < catalog->count - 1; i++)
            menu_send_input(m, MENU_DOWN);
    }
    if (step == 12) {
        screenshot(shot);
        printf("jukebox: original Library fixture passed, one Jukebox page\n");
        menu_native_focus(m, ui_widget_child(root, "Exit"));
        menu_send_input(m, MENU_ACCEPT);
    }
    step++;
    return 1;
}
