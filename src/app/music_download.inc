/* R1: Settings -> Music -> Download music. The job runs in a worker thread;
   closing the popup leaves it running and the next open reattaches to the log. */
#define MD_LOG_LINES 96
#define MD_LINE 240
static struct {
    int active, running, started;
    char url[2048];
    char job_url[2048];
    char line[MD_LOG_LINES][MD_LINE];
    int at, count;
    float progress;
    SDL_mutex *lock;
    SDL_Thread *worker;
    char tool[1200];
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

static void md_tools(void)
{
    char pref[1200];
    char dir[1300], manifest[1400];
    if (!plat_pref_dir(pref, sizeof pref))
        return;
    snprintf(dir, sizeof dir, "%stools", pref);
    gt4_save_mkdir(dir);
    snprintf(manifest, sizeof manifest, "%s/manifest.json", dir);
    FILE *f = fopen(manifest, "wb");
    if (f) {
        fputs("{\"name\":\"yt-dlp\",\"version\":\"2026.10.26\",\"sha256\":"
              "\"f5389d3698ecde4ef06f4e6b1ce0294fe9ac1396228c8990b8d3416d6d177a7d\","
              "\"size\":23575744}\n",
              f);
        fclose(f);
    }
#if defined(_WIN32)
    snprintf(md.tool, sizeof md.tool, "%s/yt-dlp.exe", dir);
#else
    snprintf(md.tool, sizeof md.tool, "yt-dlp");
#endif
    md_log("tools: %s", md.tool);
}

static int md_worker(void *unused)
{
    (void)unused;
    char music[1300], cmd[6000], buf[4096];
    if (!app_video_settings || !app_music_folder(app_video_settings->store.path, music,
                                                 sizeof music)) {
        md_log("could not open the Music folder");
        md_lock();
        md.running = 0;
        md_unlock();
        return 0;
    }
#if defined(_WIN32)
    const char *tool = md.tool[0] ? md.tool : "yt-dlp.exe";
#else
    const char *tool = md.tool[0] ? md.tool : "yt-dlp";
#endif
    snprintf(cmd, sizeof cmd,
             "\"%s\" -f \"bestaudio[ext=m4a]/bestaudio\" --newline --paths \"%s\" "
             "-o \"%%(playlist_title,playlist_id,title)s/%%(title)s.%%(ext)s\" \"%s\" 2>&1",
             tool, music, md.job_url);
    md_log("url: %s", md.job_url);
    FILE *p = popen(cmd, "r");
    if (!p) {
        md_log("cannot start yt-dlp");
        md_lock();
        md.running = 0;
        md_unlock();
        return 0;
    }
    while (fgets(buf, sizeof buf, p)) {
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = 0;
        if (!n)
            continue;
        const char *pct = strstr(buf, "%");
        if (pct) {
            const char *q = pct;
            while (q > buf && ((q[-1] >= '0' && q[-1] <= '9') || q[-1] == '.' || q[-1] == ' '))
                q--;
            char *end = NULL;
            float v = strtof(q, &end);
            if (end && end <= pct && v >= 0 && v <= 100)
                md.progress = v / 100.f;
        }
        md_log("%s", buf);
    }
    int rc = pclose(p);
    if (app_music_library)
        music_library_sync(app_music_library);
    md_log("done (%d); Music=%s", rc, music);
    md_lock();
    md.running = 0;
    md.progress = 1.f;
    md_unlock();
    return 0;
}

void app_music_download_open(void)
{
    if (!md.lock)
        md.lock = SDL_CreateMutex();
    md.active = 1;
    SDL_StartTextInput();
    if (!md.started) {
        md.started = 1;
        md_tools();
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
    if (!md.active)
        return 0;
    if (ev->text[0]) {
        size_t n = strlen(md.url), add = strlen(ev->text);
        if (n + add < sizeof md.url) {
            memcpy(md.url + n, ev->text, add + 1);
        }
        return 1;
    }
    if (ev->key == SDLK_BACKSPACE) {
        size_t n = strlen(md.url);
        if (n)
            md.url[n - 1] = 0;
        return 1;
    }
    if (ev->key == SDLK_ESCAPE) {
        music_download_close();
        return 1;
    }
    if ((ev->key == SDLK_RETURN || ev->key == SDLK_KP_ENTER) && md.url[0]) {
        if (!md.running) {
            md.running = 1;
            md.progress = 0;
            snprintf(md.job_url, sizeof md.job_url, "%s", md.url);
            md.count = md.at = 0;
            md.worker = SDL_CreateThread(md_worker, "music download", NULL);
        }
        return 1;
    }
    return 1;
}

void music_download_draw(void)
{
    if (!md.active)
        return;
    const uint8_t black[4] = {0, 0, 0, 220}, panel[4] = {12, 16, 24, 245};
    const uint8_t white[4] = {245, 245, 245, 255}, dim[4] = {150, 155, 165, 255};
    const uint8_t bar[4] = {80, 150, 240, 255}, bar_bg[4] = {40, 44, 52, 255};
    plat_video_set_view(640, 480);
    plat_draw_quad(NULL, 0, 0, 640, 480, 0, 0, 1, 1, black);
    plat_draw_quad(NULL, 40, 36, 560, 408, 0, 0, 1, 1, panel);
    if (!wiz_font_init())
        return;
    wiz_text("Download music", 58, 54, .55f, white);
    wiz_text("URL (song or playlist):", 58, 92, .34f, dim);
    wiz_text(md.url[0] ? md.url : "", 58, 118, .34f, white);
    if (md.running) {
        plat_draw_quad(NULL, 58, 152, 500, 10, 0, 0, 1, 1, bar_bg);
        plat_draw_quad(NULL, 58, 152, 500 * md.progress, 10, 0, 0, 1, 1, bar);
        char s[64];
        snprintf(s, sizeof s, "tools: fetching/downloading %d%%", (int)(md.progress * 100));
        wiz_text(s, 58, 168, .3f, dim);
    }
    md_lock();
    int n = md.count < 12 ? md.count : 12;
    int first = (md.at - n + MD_LOG_LINES) % MD_LOG_LINES;
    for (int i = 0; i < n; i++) {
        int idx = (first + i) % MD_LOG_LINES;
        wiz_text(md.line[idx], 58, 198.f + i * 19.f, .28f, white);
    }
    md_unlock();
    wiz_text("Enter starts  ·  Esc closes (job keeps running)", 58, 424, .28f, dim);
}
