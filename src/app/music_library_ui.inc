/* P: local file operations added to the disc's actual slide/MusicRoot Jukebox. */
#include "pcm_music.h"
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
static const ml_playlist *mlj_sorting;
static int mlj_shown[ML_TRACKS]; /* the Library's rows, catalog indices */
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
    if (j->catalog < 0)
        return 0;
    ml_playlist *combined = calloc(1, sizeof *combined);
    if (!combined)
        return 0;
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
    if (!ok)
        return 0;
    j->race = mlj_default_list(j, vol, ML_RACE, "Race playlist");
    j->slide = mlj_default_list(j, vol, ML_SLIDESHOW, "Slideshow playlist");
    j->folder[0] = j->folder[1] = -1;
    if (mlj_folders_on()) {
        j->folder[0] = mlj_folder_list(j, "Menu playlist");
        j->folder[1] = mlj_folder_list(j, "Replay playlist");
    }
    return j->race >= 0 && j->slide >= 0;
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
    for (int k = 0; k < 2; k++) {
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
static int app_jukebox_pcm(menu *m, int allowed)
{
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
            snprintf(information[i][0], sizeof information[i][0], "%s", catalog->tracks[i].title);
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
    const char *config = app_video_settings->store.path;
    const char *slash = strrchr(config, '/'), *back = strrchr(config, '\\');
    if (!slash || (back && back > slash))
        slash = back;
    size_t prefix = slash ? (size_t)(slash - config + 1) : 0;
    if (prefix + sizeof "Music" > sizeof folder)
        return;
    memcpy(folder, config, prefix);
    strcpy(folder + prefix, "Music");
#ifdef _WIN32
    wchar_t wide[4096];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, folder, -1, wide, 4096))
        CreateDirectoryW(wide, NULL);
#else
    mkdir(folder, 0700);
#endif
    int scanned = 0, before = music_library_list(j->library, j->catalog)->count;
    ml_add_folder(j->library, j->catalog, folder, 0, &scanned);
    snprintf(mlj_music_root, sizeof mlj_music_root, "%s", folder);
    int added = music_library_list(j->library, j->catalog)->count - before;
    printf("jukebox: Music folder=%s added=%d catalog=%d\n", folder, added,
           music_library_list(j->library, j->catalog)->count);
}
/* GT Mode -> Jukebox (or a direct slide start): before the page is entered. With no
   local song the page stays exactly the disc's, on its PS2 playlist blobs. */
static void app_jukebox_attach(menu *m)
{
    if (!m || app_jukebox.page == m)
        return;
    /* Custom Music off: the disc's own page, on its PS2 playlist blobs */
    if (!app_video_settings ||
        !moon_options_int(&app_video_settings->store, "audio.custom_music", 0, 0, 1)) {
        printf("jukebox: native disc-only entry, custom music off\n");
        return;
    }
    app_music_ensure(1);
    if (!app_music_library)
        return;
    const char *disc = studio_disc ? studio_disc : presentation_disc, *error = NULL;
    gt4_vol *vol = disc ? gt4_vol_open(disc, &error) : NULL;
    if (!vol)
        return;
    ml_jukebox j = {0};
    j.library = app_music_library;
    j.folder[0] = j.folder[1] = -1;
    menu_jukebox_folders(m, NULL, NULL);
    int local = 0;
    if (mlj_seed(&j, vol)) {
        mlj_scan(&j);
        const ml_playlist *catalog = music_library_list(j.library, j.catalog);
        for (int i = 0; i < catalog->count && !local; i++)
            local = strncmp(catalog->tracks[i].path, "gt4:", 4) != 0;
    }
    /* the folders keep their lists in the library, disc songs included */
    if (!local && j.folder[0] < 0)
        printf("jukebox: native disc-only entry\n");
    else if (mlj_bind(&j, m, vol)) {
        app_jukebox.library = j.library;
        app_jukebox.page = j.page;
        app_jukebox.race = j.race;
        app_jukebox.slide = j.slide;
        app_jukebox.catalog = j.catalog;
        app_jukebox.folder[0] = j.folder[0];
        app_jukebox.folder[1] = j.folder[1];
        app_jukebox.serial = 0;
        SDL_AtomicSet(&app_jukebox.playing, 0);
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
