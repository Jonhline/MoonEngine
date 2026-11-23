/* R1b: Settings -> Music -> Download music. The tool is fetched and verified
   on open; the job runs in a worker thread and survives closing the popup. */
#define MD_LOG_LINES 96
#define MD_LINE 240
#define MD_TOOL_BYTES 23575744ull
#define MD_TOOL_SHA "f5389d3698ecde4ef06f4e6b1ce0294fe9ac1396228c8990b8d3416d6d177a7d"
#define MD_TOOL_URL "https://github.com/yt-dlp/yt-dlp/releases/download/2026.10.26/yt-dlp.exe"
#define MD_LINUX_URL "https://github.com/yt-dlp/yt-dlp/releases/download/2026.10.26/yt-dlp_linux"
#define MD_LINUX_BYTES 41166968ull
#define MD_LINUX_SHA "767ba9dce7052aa196f8b12652f433d76de128ba168966fa84b6394d5e6c6795"

static struct {
    int active, running, started, tool_ready, tool_fetching;
    char url[2048], job_url[2048], last[MD_LINE];
    char line[MD_LOG_LINES][MD_LINE];
    int at, count;
    float progress, tool_progress;
    int tool_percent;
    double tool_mb;
    char version[32];
    char font[64];
    int has_playlist;
    SDL_mutex *lock;
    SDL_Thread *worker, *tool_worker;
    char tool[1200];
    char music_dir[1300];
} md;

static void md_lock(void) { if (md.lock) SDL_LockMutex(md.lock); }
static void md_unlock(void) { if (md.lock) SDL_UnlockMutex(md.lock); }

static void md_log(const char *fmt, ...)
{
    char text[MD_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    md_lock();
    snprintf(md.line[md.at], MD_LINE, "%s", text);
    md.at = (md.at + 1) % MD_LOG_LINES;
    if (md.count < MD_LOG_LINES)
        md.count++;
    md_unlock();
    fprintf(stderr, "music download: %s\n", text);
}

typedef void (*md_line_fn)(const char *line, void *user);

#ifdef _WIN32
static void md_win_quote(char *out, size_t size, const char *arg)
{
    size_t n = 0;
    if (n + 1 < size) out[n++] = '"';
    for (; *arg && n + 3 < size; arg++) {
        if (*arg == '"' || *arg == '\\') out[n++] = '\\';
        out[n++] = *arg;
    }
    if (n + 2 < size) { out[n++] = '"'; }
    out[n] = 0;
}
static int md_spawn(char *const argv[], md_line_fn fn, void *user)
{
    char cmd[8192] = {0};
    size_t n = 0;
    for (int i = 0; argv[i]; i++) {
        char q[2048];
        md_win_quote(q, sizeof q, argv[i]);
        int w = snprintf(cmd + n, sizeof cmd - n, "%s%s", i ? " " : "", q);
        if (w < 0 || (size_t)w >= sizeof cmd - n) return -1;
        n += (size_t)w;
    }
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(rd); CloseHandle(wr);
        return -1;
    }
    CloseHandle(wr);
    char line[MD_LINE]; size_t ln = 0, skip = 0;
    char buf[512]; DWORD got;
    while (ReadFile(rd, buf, sizeof buf, &got, NULL) && got) {
        for (DWORD i = 0; i < got; i++) {
            char c = buf[i];
            if (c == '\r' || c == '\n') {
                if (skip) { skip = 0; ln = 0; continue; }
                if (ln) { line[ln] = 0; if (fn) fn(line, user); ln = 0; }
            } else if (ln + 1 < sizeof line) line[ln++] = c;
        }
    }
    if (ln) { line[ln] = 0; if (fn) fn(line, user); }
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return (int)code;
}
static int md_verify(const char *path)
{
    wchar_t wide[1200];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 1200)) return 0;
    launcher_update_manifest m = {0};
    m.size = MD_TOOL_BYTES;
    snprintf(m.sha256, sizeof m.sha256, "%s", MD_TOOL_SHA);
    return launcher_update_verify(wide, &m);
}
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
static int md_spawn(char *const argv[], md_line_fn fn, void *user)
{
    int fds[2];
    if (pipe(fds)) return -1;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 1);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 2);
    posix_spawn_file_actions_addclose(&fa, fds[0]);
    posix_spawn_file_actions_addclose(&fa, fds[1]);
    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(fds[1]);
    if (rc) { close(fds[0]); return -1; }
    char line[MD_LINE]; size_t ln = 0, skip = 0;
    char buf[512]; ssize_t got;
    while ((got = read(fds[0], buf, sizeof buf)) > 0) {
        for (ssize_t i = 0; i < got; i++) {
            char c = buf[i];
            if (c == '\r' || c == '\n') {
                if (skip) { skip = 0; ln = 0; continue; }
                if (ln) { line[ln] = 0; if (fn) fn(line, user); ln = 0; }
            } else if (ln + 1 < sizeof line) line[ln++] = c;
        }
    }
    close(fds[0]);
    if (ln) { line[ln] = 0; if (fn) fn(line, user); }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
#endif

static void md_start(void);
static void md_tool_line(const char *line, void *user)
{
    (void)user;
    const char *pct = strstr(line, "%");
    if (pct) {
        const char *q = pct;
        while (q > line && ((q[-1] >= '0' && q[-1] <= '9') || q[-1] == '.' || q[-1] == ' ')) q--;
        char *end = NULL;
        float v = strtof(q, &end);
        if (end && end <= pct && v >= 0 && v <= 100) {
            int p = (int)v;
            if (p != md.tool_percent) {
                md.tool_percent = p;
                md.tool_progress = v / 100.f;
                md_log("tools: fetching yt-dlp %d%% (%.1f/%.1f MB)", p,
                       md.tool_mb * v / 100.0, md.tool_mb);
            }
        }
    }
    if (strstr(line, "ERROR") || strstr(line, "error"))
        md_log("tools: %s", line);
}

#ifndef _WIN32
static char md_hash[65];
static char md_version[64];
static void md_hash_line(const char *line, void *user)
{
    (void)user;
    if (strlen(line) >= 64) { memcpy(md_hash, line, 64); md_hash[64] = 0; }
}
static void md_version_line(const char *line, void *user)
{
    (void)user;
    if (*line) snprintf(md_version, sizeof md_version, "%s", line);
}
static int md_linux_verify(const char *path)
{
    md_hash[0] = 0;
    char *argv[] = {"sha256sum", (char *)path, NULL};
    md_spawn(argv, md_hash_line, NULL);
    return md_hash[0] && !strcasecmp(md_hash, MD_LINUX_SHA);
}
static void md_linux_version(int fetched)
{
    if (fetched) {
        snprintf(md.version, sizeof md.version, "2026.10.26");
        return;
    }
    md_version[0] = 0;
    char *argv[] = {"yt-dlp", "--version", NULL};
    md_spawn(argv, md_version_line, NULL);
    snprintf(md.version, sizeof md.version, "%s", md_version[0] ? md_version : "unknown");
}
#endif

static int md_tool_worker(void *unused)
{
    (void)unused;
#ifdef _WIN32
    md.tool_mb = 22.5;
    if (md_verify(md.tool)) {
        snprintf(md.version, sizeof md.version, "2026.10.26");
        md_log("tools: yt-dlp sha256 ok (version 2026.10.26)");
        md.tool_ready = 1;
        md.tool_fetching = 0;
        return 0;
    }
    remove(md.tool);
    md.tool_percent = -1;
    md_log("tools: fetching yt-dlp");
    char *argv[] = {"curl.exe", "-L", "--fail", "-o", md.tool, MD_TOOL_URL, NULL};
    int rc = md_spawn(argv, md_tool_line, NULL);
    if (rc == 0 && md_verify(md.tool)) {
        snprintf(md.version, sizeof md.version, "2026.10.26");
        md_log("tools: yt-dlp sha256 ok (version 2026.10.26)");
        md.tool_ready = 1;
    } else {
        remove(md.tool);
        md_log("tools: yt-dlp fetch failed (curl %d) or sha256 mismatch; removed", rc);
    }
    md.tool_fetching = 0;
#else
    md.tool_mb = 41.2;
    if (md_linux_verify(md.tool)) {
        chmod(md.tool, 0755);
        md_linux_version(1);
        md_log("tools: yt-dlp sha256 ok (version 2026.10.26)");
        md.tool_ready = 1;
    } else {
        remove(md.tool);
        md.tool_percent = -1;
        md_log("tools: fetching yt-dlp");
        char *argv[] = {"curl", "-L", "--fail", "-o", md.tool, MD_LINUX_URL, NULL};
        int rc = md_spawn(argv, md_tool_line, NULL);
        if (rc == 0) chmod(md.tool, 0755);
        if (rc == 0 && md_linux_verify(md.tool)) {
            md_linux_version(1);
            md_log("tools: yt-dlp sha256 ok (version 2026.10.26)");
            md.tool_ready = 1;
        } else {
            remove(md.tool);
            snprintf(md.tool, sizeof md.tool, "yt-dlp");
            md_linux_version(0);
            md_log("tools: fetch failed; using yt-dlp on PATH (version %s)", md.version);
            md.tool_ready = 1;
        }
    }
    md.tool_fetching = 0;
    if (getenv("MOON_DOWNLOAD_START")) md_start();
#endif
    return 0;
}

static void md_tools_begin(void)
{
    char pref[1200], dir[1300], manifest[1400];
    if (!plat_pref_dir(pref, sizeof pref)) return;
    snprintf(dir, sizeof dir, "%stools", pref);
    gt4_save_mkdir(dir);
    snprintf(manifest, sizeof manifest, "%s/manifest.json", dir);
    FILE *f = fopen(manifest, "wb");
    if (f) {
        fputs("{\"name\":\"yt-dlp\",\"version\":\"2026.10.26\",\"sha256\":\"" MD_TOOL_SHA
              "\",\"size\":23575744}\n", f);
        fclose(f);
    }
#ifdef _WIN32
    snprintf(md.tool, sizeof md.tool, "%s/yt-dlp.exe", dir);
#else
    snprintf(md.tool, sizeof md.tool, "%s/yt-dlp", dir);
#endif
    md_log("tools: %s", md.tool);
    md.tool_fetching = 1;
    md.tool_worker = SDL_CreateThread(md_tool_worker, "yt-dlp fetch", NULL);
}

static int md_worker(void *);
static int md_url_ok(const char *);
static void md_start(void)
{
    if (md.running || !md.tool_ready) return;
    if (!md_url_ok(md.url)) {
        md_log("yt-dlp error: enter an http:// or https:// URL with no control characters");
        return;
    }
    md.running = 1;
    md.progress = 0;
    md.count = md.at = 0;
    snprintf(md.job_url, sizeof md.job_url, "%s", md.url);
    md.url[0] = 0;
    md.worker = SDL_CreateThread(md_worker, "music download", NULL);
}

static int md_url_ok(const char *u)
{
    if (!u) return 0;
    size_t n = strlen(u);
    if (n < 8 || n >= sizeof md.url) return 0;
    if (strncmp(u, "http://", 7) && strncmp(u, "https://", 8)) return 0;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)u[i] < 0x20 || u[i] == 0x7f) return 0;
    return 1;
}

static void md_print_line(const char *line, void *user)
{
    (void)user;
    if (*line) md.has_playlist = 1;
}

static void md_job_line(const char *line, void *user)
{
    (void)user;
    snprintf(md.last, sizeof md.last, "%s", line);
    const char *pct = strstr(line, "%");
    if (pct) {
        const char *q = pct;
        while (q > line && ((q[-1] >= '0' && q[-1] <= '9') || q[-1] == '.' || q[-1] == ' ')) q--;
        char *end = NULL;
        float v = strtof(q, &end);
        if (end && end <= pct && v >= 0 && v <= 100)
            md.progress = v / 100.f;
    }
    md_log("%s", line);
}

static int md_worker(void *unused)
{
    (void)unused;
    char music[1300];
    if (md.music_dir[0])
        snprintf(music, sizeof music, "%s", md.music_dir);
    else if (!app_video_settings || !app_music_folder(app_video_settings->store.path, music,
                                                      sizeof music)) {
        md_log("could not open the Music folder");
        md_lock(); md.running = 0; md_unlock();
        return 0;
    }
    md.last[0] = 0;
    char *argv[] = {md.tool, "-f", "bestaudio[ext=m4a]/bestaudio", "--newline",
                    "--paths", music, "-o",
                    "%(playlist_title|)s%(playlist_title&/|)s%(title)s.%(ext)s",
                    md.job_url, NULL};
    md_log("url: %s", md.job_url);
    int rc = md_spawn(argv, md_job_line, NULL);
    if (rc < 0) md_log("yt-dlp error: could not start %s", md.tool);
    if (app_music_library)
        music_library_sync(app_music_library);
    if (md.last[0])
        md_log("yt-dlp: %s", md.last);
    app_music_ensure(1);
    if (!app_jukebox.library && app_music_library) {
        app_jukebox.library = app_music_library;
        app_jukebox.catalog = music_library_catalog(app_music_library);
    }
    if (app_jukebox.library && app_jukebox.catalog >= 0) {
        const ml_playlist *before = music_library_list(app_jukebox.library, app_jukebox.catalog);
        int was = before ? (int)before->count : 0;
        mlj_scan(&app_jukebox);
        const ml_playlist *after = music_library_list(app_jukebox.library, app_jukebox.catalog);
        int now = after ? (int)after->count : 0;
        char folder[1300] = "";
        if (md.music_dir[0])
            snprintf(folder, sizeof folder, "%s", md.music_dir);
        else if (app_video_settings)
            app_music_folder(app_video_settings->store.path, folder, sizeof folder);
        int found = 0;
        {
            char *hit = strstr(folder, " [");
            (void)hit;
            DIR *d = opendir(folder);
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (e->d_name[0] == '.') continue;
                    size_t l = strlen(e->d_name);
                    if (l > 4 && (!strcasecmp(e->d_name + l - 4, ".mp3") ||
                                  !strcasecmp(e->d_name + l - 4, ".m4a") ||
                                  !strcasecmp(e->d_name + l - 4, ".ogg") ||
                                  !strcasecmp(e->d_name + l - 4, ".flac")))
                        found++;
                }
                closedir(d);
            }
        }
        md_log("Music: %d tracks added, %d in the library (folder %s)", now - was, found, folder);
    }
    md_lock(); md.running = 0; md.progress = 1.f; md_unlock();
    return 0;
}

void app_music_download_open(void)
{
    if (!md.lock)
        md.lock = SDL_CreateMutex();
    md.active = 1;
    SDL_StartTextInput();
    if ((!app_video_settings || !app_video_settings->store.path[0]) && !md.music_dir[0]) {
        char pref[1200];
        if (plat_pref_dir(pref, sizeof pref)) {
            snprintf(md.music_dir, sizeof md.music_dir, "%sMusic", pref);
            gt4_save_mkdir(md.music_dir);
        }
    }
    md.font[0] = 0;
    if (app_video_settings && app_video_settings->rows[34]) {
        ui_widget *label = ui_widget_child(app_video_settings->rows[34], "label");
        const char *face = label && label->src ? mp_string(label->src, "font") : NULL;
        if (face) snprintf(md.font, sizeof md.font, "%s", face);
    }
    const char *prefill = getenv("MOON_DOWNLOAD_URL");
    if (prefill && !md.url[0])
        snprintf(md.url, sizeof md.url, "%s", prefill);
    if (!md.started) {
        md.started = 1;
        md_tools_begin();
        md_log("Enter a song or playlist URL, then Enter. Esc closes; the job keeps running.");
    }
}

int music_download_active(void) { return md.active; }

void music_download_close(void)
{
    md.active = 0;
    SDL_StopTextInput();
}

int music_download_event(plat_key_event *ev)
{
    if (!md.active) return 0;
    if (ev->text[0]) {
        size_t n = strlen(md.url), add = strlen(ev->text);
        if (n + add < sizeof md.url) memcpy(md.url + n, ev->text, add + 1);
        return 1;
    }
    if (ev->key == SDLK_BACKSPACE) {
        size_t n = strlen(md.url);
        if (n) md.url[n - 1] = 0;
        return 1;
    }
    if (ev->key == SDLK_ESCAPE) { music_download_close(); return 1; }
    if ((ev->key == SDLK_RETURN || ev->key == SDLK_KP_ENTER) && md.url[0]) {
        if (!md.tool_ready) {
            md_log(md.tool_fetching ? "tools: still fetching yt-dlp" : "tools: not ready");
        } else
            md_start();
        return 1;
    }
    return 1;
}

void music_download_draw(void)
{
    if (!md.active) return;
    const uint8_t overlay[4] = {0, 0, 0, 160}, panel[4] = {150, 150, 155, 255};
    const uint8_t title[4] = {60, 58, 56, 255}, black[4] = {20, 20, 22, 255};
    const uint8_t white[4] = {245, 245, 245, 255}, dim[4] = {90, 90, 95, 255};
    const uint8_t gold[4] = {180, 160, 110, 255}, track[4] = {105, 105, 110, 255};
    ui_project *p = app_video_settings && app_video_settings->owner
                        ? menu_ui(app_video_settings->owner) : NULL;
    plat_video_set_view(640, 480);
    plat_draw_quad(NULL, 0, 0, 640, 480, 0, 0, 1, 1, overlay);
    plat_draw_quad(NULL, 70, 52, 500, 372, 0, 0, 1, 1, panel);
    plat_draw_quad(NULL, 70, 52, 500, 30, 0, 0, 1, 1, title);
#define MDT(x, y, sc, col, str) do { \
        if (p && md.font[0] && ui_string_height(p, md.font, (sc)) > 0) \
            ui_draw_string(p, md.font, (x), (y), (sc), 0, (col), (str)); \
        else { wiz_font_init(); wiz_text((str), (x), (y), (sc) * 1.2f, (col)); } \
    } while (0)
    MDT(84, 66, .46f, white, "Download music");
    MDT(86, 104, .34f, black, "URL");
    MDT(130, 104, .34f, black, md.url[0] ? md.url : "(paste a song or playlist URL)");
    if (md.running || md.tool_fetching) {
        float pr = md.running ? md.progress : md.tool_progress;
        plat_draw_quad(NULL, 86, 320, 468, 10, 0, 0, 1, 1, track);
        plat_draw_quad(NULL, 86, 320, 468 * pr, 10, 0, 0, 1, 1, gold);
        char s[96];
        snprintf(s, sizeof s, "%s %d%%", md.running ? "download" : "tools: fetching yt-dlp",
                 (int)(pr * 100));
        MDT(86, 340, .30f, black, s);
    }
    md_lock();
    int n = md.count < 10 ? md.count : 10;
    int first = (md.at - n + MD_LOG_LINES) % MD_LOG_LINES;
    for (int i = 0; i < n; i++) {
        int idx = (first + i) % MD_LOG_LINES;
        char show[76];
        snprintf(show, sizeof show, "%.72s", md.line[idx]);
        MDT(86, 142 + i * 17, .30f, black, show);
    }
    md_unlock();
    const uint8_t edge[4] = {115, 115, 120, 255}, face[4] = {195, 195, 200, 255};
    plat_draw_quad(NULL, 392, 372, 140, 26, 0, 0, 1, 1, edge);
    plat_draw_quad(NULL, 393, 373, 138, 24, 0, 0, 1, 1, face);
    MDT(400, 386, .34f, black, "Close");
    MDT(86, 400, .30f, dim, "Enter starts  ·  Esc closes (job keeps running)");
#undef MDT
}
