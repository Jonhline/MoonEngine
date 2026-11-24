/* P: the first-run preparation step for the movie decoder (see the header).
   Nothing here is game simulation or disc data; it is host housekeeping, so the
   menu's cutscene and its car work on a machine that only received the exe. */
#include "prepare_ffmpeg.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <SDL.h>
#include "launcher_text.h"
#include "launcher_fonts_data.h"
#include "launcher_update.h"
#include "brand_logo.h"
#include "miniz.h"

/* The rolling "latest" release of BtbN FFmpeg-Builds. The asset name is the same
   for every master build, so the URL does not pin a version that will vanish. */
#define FFMPEG_URL_DEFAULT \
    L"https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip"
#define FFMPEG_MIN_BYTES (20ull * 1024 * 1024)     /* reject an HTML error page */
#define FFMPEG_MAX_BYTES (600ull * 1024 * 1024)    /* reject a runaway response */
#define RETRY_SECONDS 86400                        /* remember a failure for a day */

#define PREP_W 960   /* the launcher's window: the setup that follows opens in the same place */
#define PREP_H 720

enum { ST_CHECK = 0, ST_DOWNLOAD, ST_EXTRACT, ST_VERIFY, ST_DONE, ST_FAILED, ST_CANCELLED };
enum { ERR_NONE = 0, ERR_NETWORK, ERR_HTTP, ERR_WRITE, ERR_SMALL, ERR_EXTRACT, ERR_MOVE };

typedef struct {
    volatile LONG state, cancel, err, leave;
    volatile LONG64 got;          /* bytes written to the zip so far */
    volatile LONG64 total;        /* Content-Length, 0 when unknown */
    wchar_t url[4096], user[MAX_PATH], workdir[MAX_PATH];
} prep_job;

static void prep_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}

static LONG prep_state(prep_job *j)
{
    return InterlockedCompareExchange(&j->state, 0, 0);
}
static LONG prep_err(prep_job *j)
{
    return InterlockedCompareExchange(&j->err, 0, 0);
}
static LONG64 prep_bytes(volatile LONG64 *v)
{
    return InterlockedCompareExchange64(v, 0, 0);
}

/* ---- the HTTP(S) transfer (WinHTTP, redirects followed) ---- */
static HINTERNET prep_open(HINTERNET *session, HINTERNET *connection, const wchar_t *url, int *err)
{
    wchar_t host[512], path[8192];
    URL_COMPONENTS u = {0};
    u.dwStructSize = sizeof u;
    u.lpszHostName = host; u.dwHostNameLength = 512;
    u.lpszUrlPath = path;  u.dwUrlPathLength = 8192;
    u.dwUserNameLength = u.dwPasswordLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url, 0, 0, &u) || u.nScheme != INTERNET_SCHEME_HTTPS) {
        prep_log("prepare: refusing an address that is not https\n");
        *err = ERR_NETWORK;
        return NULL;
    }
    *session = WinHttpOpen(L"MoonEngine/" L"prepare", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!*session) {
        *err = ERR_NETWORK;
        return NULL;
    }
    WinHttpSetTimeouts(*session, 10000, 10000, 30000, 30000);
    *connection = WinHttpConnect(*session, host, u.nPort, 0);
    if (!*connection) {
        *err = ERR_NETWORK;
        return NULL;
    }
    HINTERNET r = WinHttpOpenRequest(*connection, L"GET", path, NULL, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!r) {
        *err = ERR_NETWORK;
        return NULL;
    }
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;   /* github -> the asset host */
    WinHttpSetOption(r, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof redirect);
    if (!WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(r, NULL)) {
        prep_log("prepare: the connection failed\n");
        *err = ERR_NETWORK;
        WinHttpCloseHandle(r);
        return NULL;
    }
    DWORD status = 0, n = sizeof status;
    if (!WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL,
                             &status, &n, NULL) || status != 200) {
        prep_log("prepare: the server answered %lu\n", (unsigned long)status);
        *err = ERR_HTTP;
        WinHttpCloseHandle(r);
        return NULL;
    }
    return r;
}

/* The worker downloads, extracts, verifies and places; the window below only
   reads the job's atomics, so no lock is needed. */
static DWORD WINAPI prep_worker(void *context)
{
    prep_job *j = context;
    HINTERNET session = NULL, connection = NULL;
    int err = ERR_NONE;
    HINTERNET r = prep_open(&session, &connection, j->url, &err);
    if (!r) {
        InterlockedExchange(&j->err, err ? err : ERR_NETWORK);
        goto out;
    }
    {
        DWORD length = 0, ln = sizeof length;
        if (WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL,
                                &length, &ln, NULL) && length > 0 && length <= FFMPEG_MAX_BYTES)
            InterlockedExchange64((volatile LONG64 *)&j->total, (LONG64)length);
    }
    InterlockedExchange(&j->state, ST_DOWNLOAD);
    wchar_t zip[MAX_PATH];
    swprintf(zip, MAX_PATH, L"%ls\\ffmpeg.zip", j->workdir);
    HANDLE f = CreateFileW(zip, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&j->err, ERR_WRITE);
        goto out;
    }
    unsigned char buf[65536];
    int ok = 1;
    for (;;) {
        if (InterlockedCompareExchange(&j->cancel, 1, 1)) {
            ok = 0;
            break;
        }
        DWORD got = 0, written = 0;
        if (!WinHttpReadData(r, buf, sizeof buf, &got)) {
            ok = 0;
            break;
        }
        if (!got)
            break;
        if (!WriteFile(f, buf, got, &written, NULL) || written != got) {
            ok = 0;
            break;
        }
        LONG64 total = InterlockedExchangeAdd64((volatile LONG64 *)&j->got, (LONG64)got) + got;
        if (total > FFMPEG_MAX_BYTES) {
            ok = 0;
            break;
        }
    }
    CloseHandle(f);
    if (!ok) {
        if (InterlockedCompareExchange(&j->cancel, 0, 0))
            InterlockedExchange(&j->state, ST_CANCELLED);
        else
            InterlockedExchange(&j->err, ERR_NETWORK);
        goto out;
    }
    if (InterlockedCompareExchange(&j->cancel, 0, 0)) {
        InterlockedExchange(&j->state, ST_CANCELLED);
        goto out;
    }
    LARGE_INTEGER size = {{0}};
    HANDLE zf = CreateFileW(zip, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    int zip_ok = zf != INVALID_HANDLE_VALUE && GetFileSizeEx(zf, &size) &&
                 (uint64_t)size.QuadPart >= FFMPEG_MIN_BYTES &&
                 (uint64_t)size.QuadPart <= FFMPEG_MAX_BYTES;
    if (zf != INVALID_HANDLE_VALUE)
        CloseHandle(zf);
    if (!zip_ok) {
        prep_log("prepare: the archive is too small or missing (%lld bytes)\n",
                 (long long)size.QuadPart);
        InterlockedExchange(&j->err, ERR_SMALL);
        goto out;
    }
    InterlockedExchange(&j->state, ST_EXTRACT);
    {
        wchar_t cmd[8192];
        swprintf(cmd, 8192, L"tar.exe -xf \"%ls\" -C \"%ls\"", zip, j->workdir);
        STARTUPINFOW si = {sizeof si};
        PROCESS_INFORMATION pi = {0};
        if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            prep_log("prepare: could not run tar.exe to unpack the archive\n");
            InterlockedExchange(&j->err, ERR_EXTRACT);
            goto out;
        }
        CloseHandle(pi.hThread);
        WaitForSingleObject(pi.hProcess, 600000);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        if (code != 0) {
            prep_log("prepare: tar.exe failed to unpack the archive (exit %lu)\n",
                     (unsigned long)code);
            InterlockedExchange(&j->err, ERR_EXTRACT);
            goto out;
        }
    }
    InterlockedExchange(&j->state, ST_VERIFY);
    {
        wchar_t pattern[MAX_PATH], ffmpeg[MAX_PATH];
        swprintf(pattern, MAX_PATH, L"%ls\\*", j->workdir);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern, &fd);
        int found = 0;
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                    !wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
                    continue;
                wchar_t cand[MAX_PATH];
                swprintf(cand, MAX_PATH, L"%ls\\%ls\\bin\\ffmpeg.exe", j->workdir, fd.cFileName);
                if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
                    wcsncpy(ffmpeg, cand, MAX_PATH - 1);
                    ffmpeg[MAX_PATH - 1] = 0;
                    found = 1;
                    break;
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        if (!found) {
            prep_log("prepare: the archive held no bin\\ffmpeg.exe\n");
            InterlockedExchange(&j->err, ERR_EXTRACT);
            goto out;
        }
        HANDLE mf = CreateFileW(ffmpeg, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        char magic[2] = {0, 0};
        DWORD rn = 0;
        int valid = mf != INVALID_HANDLE_VALUE && GetFileSizeEx(mf, &size) &&
                    (uint64_t)size.QuadPart >= FFMPEG_MIN_BYTES && ReadFile(mf, magic, 2, &rn, NULL) &&
                    rn == 2 && magic[0] == 'M' && magic[1] == 'Z';
        if (mf != INVALID_HANDLE_VALUE)
            CloseHandle(mf);
        if (!valid) {
            prep_log("prepare: the unpacked ffmpeg.exe failed verification\n");
            InterlockedExchange(&j->err, ERR_SMALL);
            goto out;
        }
        if (!MoveFileW(ffmpeg, j->user) &&
            !(CopyFileW(ffmpeg, j->user, FALSE) && DeleteFileW(ffmpeg))) {
            prep_log("prepare: could not place ffmpeg.exe in the user folder\n");
            InterlockedExchange(&j->err, ERR_MOVE);
            goto out;
        }
        prep_log("prepare: placed %ls (%lld bytes)\n", j->user, (long long)size.QuadPart);
    }
    InterlockedExchange(&j->state, ST_DONE);
out:
    /* an error names the failure (the window shows it, then the game goes on) */
    if (prep_err(j) && prep_state(j) != ST_CANCELLED)
        InterlockedExchange(&j->state, ST_FAILED);
    if (r)
        WinHttpCloseHandle(r);
    if (connection)
        WinHttpCloseHandle(connection);
    if (session)
        WinHttpCloseHandle(session);
    return 0;
}

/* ---- the small window ---- */
typedef struct {
    SDL_Window *window;
    SDL_Renderer *ren;
    lt_face *face_body, *face_head;
    int w, h;
} prep_ui;

/* The launcher's first-run look (docs/UI_ART_DIRECTION.md, "Without the disc"): the white
   room, the graphite band with a fine light edge and the black help strip, in the home
   bar's 640x448 proportions, the bundled Barlow. Nothing here comes from the game. */
static void p_fill(prep_ui *u, float x, float y, float w, float h, int r, int g, int b, int a)
{
    SDL_SetRenderDrawColor(u->ren, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_Rect rc = {(int)x, (int)y, (int)(x + w) - (int)x, (int)(y + h) - (int)y};
    SDL_RenderFillRect(u->ren, &rc);
}
static void p_ramp(prep_ui *u, float y0, float y1, const int top[4], const int bot[4])
{
    for (int y = (int)y0; y < (int)y1; y++) {
        float t = y1 > y0 ? (y - y0) / (y1 - y0) : 0;
        SDL_SetRenderDrawColor(u->ren, (Uint8)(top[0] + (bot[0] - top[0]) * t),
                               (Uint8)(top[1] + (bot[1] - top[1]) * t),
                               (Uint8)(top[2] + (bot[2] - top[2]) * t),
                               (Uint8)(top[3] + (bot[3] - top[3]) * t));
        SDL_RenderDrawLine(u->ren, 0, y, u->w - 1, y);
    }
}
/* text on a baseline; centred on cx when centre is set */
static void p_text(prep_ui *u, lt_font *font, float x, float base, const char *s, int centre, int r,
                   int g, int b, int a)
{
    SDL_Color c = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
    if (centre)
        x -= lt_width(font, s, 0) / 2.f;
    lt_draw(u->ren, font, (int)x, (int)base - lt_ascent(font), s, 0, c);
}
static void prep_draw(prep_ui *u, prep_job *j)
{
    const float S = fminf(u->w / 640.f, u->h / 448.f), ox = (u->w - 640.f * S) * .5f,
                oy = u->h - 448.f * S;
    const float top = oy + 363.f * S, strip = oy + 418.f * S;
    lt_font *mark = lt_font_size(u->face_body, (int)(26.4f * S));
    lt_font *note = lt_font_size(u->face_body, (int)(10.6f * S));
    lt_font *line_font = lt_font_size(u->face_head, (int)(15 * S));
    lt_font *help = lt_font_size(u->face_body, (int)(10.5f * S));
    SDL_SetRenderDrawBlendMode(u->ren, SDL_BLENDMODE_BLEND);
    /* the room: wall, the sweep, the floor */
    const int wall[4] = {254, 254, 253, 255}, ground[4] = {242, 242, 245, 255};
    float y0 = u->h * 250.f / 448.f, y1 = u->h * 400.f / 448.f;
    p_fill(u, 0, 0, (float)u->w, y0, wall[0], wall[1], wall[2], 255);
    p_ramp(u, y0, y1, wall, ground);
    p_fill(u, 0, y1, (float)u->w, u->h - y1, ground[0], ground[1], ground[2], 255);
    /* the corner: the logo, where the setup has it */
    {
        static SDL_Texture *logo;
        static int lw, lh;
        if (!logo) {
            const uint8_t *rgba = brand_logo_rgba(0, &lw, &lh);
            if (rgba && (logo = SDL_CreateTexture(u->ren, SDL_PIXELFORMAT_ABGR8888,
                                                  SDL_TEXTUREACCESS_STATIC, lw, lh))) {
                SDL_UpdateTexture(logo, NULL, rgba, lw * 4);
                SDL_SetTextureBlendMode(logo, SDL_BLENDMODE_BLEND);
            }
        }
        const float h = 44 * S, w = lh > 0 ? h * lw / lh : 0;
        if (logo) {
            SDL_Rect rc = {(int)(16 * S), (int)(12 * S), (int)w, (int)h};
            SDL_RenderCopy(u->ren, logo, NULL, &rc);
        } else
            p_text(u, mark, 16 * S, 38 * S, "MOON ENGINE", 0, 38, 40, 45, 255);
        p_text(u, note, logo ? 22 * S + w : 17 * S, (logo ? 54 : 52) * S, MOON_VERSION_LABEL, 0, 110,
               113, 120, 255);
    }

    LONG state = prep_state(j);
    LONG err = prep_err(j);
    LONG64 got = prep_bytes((volatile LONG64 *)&j->got);
    LONG64 total = prep_bytes((volatile LONG64 *)&j->total);
    char line[256];
    int pct = total > 0 ? (int)(got * 100 / total) : -1;
    switch (state) {
    case ST_DOWNLOAD:
        if (pct >= 0)
            snprintf(line, sizeof line, "Downloading the movie decoder   %d%%   %.1f / %.1f MB", pct,
                     got / 1048576.0, total / 1048576.0);
        else
            snprintf(line, sizeof line, "Downloading the movie decoder   %.1f MB", got / 1048576.0);
        break;
    case ST_EXTRACT: snprintf(line, sizeof line, "Unpacking the movie decoder"); break;
    case ST_VERIFY:  snprintf(line, sizeof line, "Checking the movie decoder"); break;
    case ST_DONE:    snprintf(line, sizeof line, "Ready"); break;
    case ST_FAILED:
        snprintf(line, sizeof line,
                 "%s", err == ERR_HTTP  ? "Download failed: the server refused it"
                       : err == ERR_SMALL ? "Download failed: the file was not valid"
                       : err == ERR_EXTRACT ? "Download failed: it could not be unpacked"
                       : err == ERR_MOVE ? "Download failed: it could not be saved"
                                         : "Download failed: no connection?");
        break;
    case ST_CANCELLED: snprintf(line, sizeof line, "Download skipped"); break;
    default:           snprintf(line, sizeof line, "Looking for the movie decoder"); break;
    }
    /* the band: graphite, a fine light edge; the step and its progress inside */
    const int band_top[4] = {62, 64, 70, 235}, band_bot[4] = {32, 34, 38, 235};
    p_ramp(u, top, strip, band_top, band_bot);
    p_fill(u, 0, top, (float)u->w, fmaxf(1.f, S * .7f), 255, 255, 255, 110);
    int failed = state == ST_FAILED;
    p_text(u, line_font, u->w * .5f + S, oy + 386.f * S + S, line, 1, 0, 0, 0, 120);
    p_text(u, line_font, u->w * .5f, oy + 386.f * S, line, 1, failed ? 235 : 236,
           failed ? 120 : 238, failed ? 110 : 241, 255);
    float bar_x = ox + 160.f * S, bar_w = 320.f * S, bar_y = oy + 398.f * S,
          bar_h = fmaxf(2.f, 3.f * S);
    p_fill(u, bar_x, bar_y, bar_w, bar_h, 255, 255, 255, 40);
    float fill = 0;
    if (state == ST_DOWNLOAD)
        fill = pct >= 0 ? bar_w * pct / 100.f : bar_w / 4.f;
    else if (state == ST_EXTRACT || state == ST_VERIFY || state == ST_DONE)
        fill = bar_w;
    if (fill > 0)
        p_fill(u, bar_x, bar_y, fill, bar_h, 255, 255, 255, 230);
    /* the help strip: black, one line, centred */
    p_fill(u, 0, strip, (float)u->w, (float)u->h - strip, 0, 0, 0, 240);
    p_fill(u, 0, strip, (float)u->w, fmaxf(1.f, S * .5f), 70, 72, 76, 255);
    p_text(u, help, u->w * .5f, oy + 437.f * S,
           failed || state == ST_CANCELLED
               ? "The game starts without its movies; it tries again tomorrow. Any key: continue."
               : "ffmpeg (BtbN FFmpeg-Builds, win64-gpl) plays the game's movies. Fetched once.   Esc: skip",
           1, 214, 216, 220, 255);
    SDL_RenderPresent(u->ren);
}
static void prep_capture(prep_ui *u, const char *path)
{
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, u->w, u->h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf)
        return;
    if (SDL_RenderReadPixels(u->ren, NULL, SDL_PIXELFORMAT_RGBA32, surf->pixels, surf->pitch) == 0) {
        unsigned char *px = malloc((size_t)u->w * u->h * 4);
        if (px) {
            for (int row = 0; row < u->h; row++)
                memcpy(px + (size_t)row * u->w * 4,
                       (unsigned char *)surf->pixels + (size_t)row * surf->pitch, (size_t)u->w * 4);
            size_t len = 0;
            void *png = tdefl_write_image_to_png_file_in_memory(px, (size_t)u->w, (size_t)u->h, 4, &len);
            FILE *f = png ? fopen(path, "wb") : NULL;
            if (f) {
                fwrite(png, 1, len, f);
                fclose(f);
                prep_log("prepare: wrote %s (%dx%d)\n", path, u->w, u->h);
            }
            mz_free(png);
            free(px);
        }
    }
    SDL_FreeSurface(surf);
}

static void prep_show(prep_job *j, const char *shot)
{
    prep_ui u = {0};
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        prep_log("prepare: no window: %s\n", SDL_GetError());
        return;
    }
    u.window = SDL_CreateWindow("MOON ENGINE", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, PREP_W, PREP_H, SDL_WINDOW_SHOWN);
    if (u.window) {
        SDL_GetWindowSize(u.window, &u.w, &u.h);
        u.ren = SDL_CreateRenderer(u.window, -1, SDL_RENDERER_ACCELERATED);
        if (!u.ren)
            u.ren = SDL_CreateRenderer(u.window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (u.ren) {
        u.face_body = lt_face_open(launcher_ttf_BARLOW_REGULAR, (int)sizeof launcher_ttf_BARLOW_REGULAR);
        u.face_head = lt_face_open(launcher_ttf_BARLOWCONDENSED_SEMIBOLD,
                                   (int)sizeof launcher_ttf_BARLOWCONDENSED_SEMIBOLD);
    }
    int shot_taken = 0, last_bucket = -1;
    ULONGLONG finished_at = 0;
    while (!InterlockedCompareExchange(&j->leave, 0, 0)) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                InterlockedExchange(&j->cancel, 1);
                InterlockedExchange(&j->leave, 1);
            } else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    InterlockedExchange(&j->cancel, 1);
                    InterlockedExchange(&j->leave, 1);
                } else if (prep_state(j) == ST_FAILED || prep_state(j) == ST_CANCELLED) {
                    InterlockedExchange(&j->leave, 1);
                }
            }
        }
        LONG state = prep_state(j);
        LONG64 got = prep_bytes((volatile LONG64 *)&j->got);
        LONG64 total = prep_bytes((volatile LONG64 *)&j->total);
        if (state == ST_DOWNLOAD && total > 0) {         /* progress named in the log, once per 10% */
            int bucket = (int)(got * 10 / total);
            if (bucket != last_bucket) {
                last_bucket = bucket;
                prep_log("prepare: downloading ffmpeg: %d%% (%.1f MB / %.1f MB)\n", (int)(got * 100 / total),
                         got / 1048576.0, total / 1048576.0);
            }
        }
        if (u.ren && u.face_body && u.face_head)
            prep_draw(&u, j);
        if (shot && !shot_taken && u.ren && u.face_body && u.face_head) {
            int ready = (state == ST_DOWNLOAD && got > 0 &&
                         (total <= 0 ? got >= 8 * 1024 * 1024 : got * 100 / total >= 5)) ||
                        state == ST_EXTRACT || state == ST_VERIFY || state == ST_DONE ||
                        state == ST_FAILED || state == ST_CANCELLED;
            if (ready) {
                prep_capture(&u, shot);
                shot_taken = 1;
            }
        }
        if (state == ST_DONE) {
            if (!finished_at)
                finished_at = GetTickCount64();
            if (GetTickCount64() - finished_at > 600)
                break;
        } else if (state == ST_FAILED || state == ST_CANCELLED) {
            if (!finished_at)
                finished_at = GetTickCount64();
            if (GetTickCount64() - finished_at > 10000)   /* "continue anyway" */
                break;
        }
        SDL_Delay(16);
    }
    if (u.face_body)
        lt_close(u.face_body);
    if (u.face_head)
        lt_close(u.face_head);
    if (u.ren)
        SDL_DestroyRenderer(u.ren);
    if (u.window)
        SDL_DestroyWindow(u.window);
    SDL_Quit();
}

/* ---- the entry point ---- */
static void prep_recursive_delete(const wchar_t *path)
{
    wchar_t pattern[MAX_PATH];
    swprintf(pattern, MAX_PATH, L"%ls\\*", path);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
                continue;
            wchar_t child[MAX_PATH];
            swprintf(child, MAX_PATH, L"%ls\\%ls", path, fd.cFileName);
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                prep_recursive_delete(child);
            else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                RemoveDirectoryW(child);
            else
                DeleteFileW(child);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(path);
}
static int prep_stamp_fresh(const wchar_t *stamp)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(stamp, GetFileExInfoStandard, &a))
        return 0;
    ULARGE_INTEGER then, now;
    then.LowPart = a.ftLastWriteTime.dwLowDateTime;
    then.HighPart = a.ftLastWriteTime.dwHighDateTime;
    GetSystemTimeAsFileTime((FILETIME *)&now);
    return now.QuadPart > then.QuadPart &&
           (now.QuadPart - then.QuadPart) < (ULONGLONG)RETRY_SECONDS * 10000000ull;
}
static void prep_stamp_write(const wchar_t *stamp)
{
    HANDLE f = CreateFileW(stamp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    char when[64];
    int n = snprintf(when, sizeof when, "%llu\n", (unsigned long long)time(NULL));
    DWORD written = 0;
    WriteFile(f, when, (DWORD)n, &written, NULL);
    CloseHandle(f);
}
static void prep_make_dirs(wchar_t *path)
{
    for (wchar_t *at = path; *at; at++) {
        if ((at > path + 2 && (*at == L'\\' || *at == L'/')) || !at[1]) {
            wchar_t c = *at;
            int end = !at[1];
            if (!end)
                *at = 0;
            CreateDirectoryW(path, NULL);
            if (!end)
                *at = c;
        }
    }
}

void launcher_prepare_ffmpeg(void)
{
    char pref[1200];
    if (!plat_pref_dir(pref, sizeof pref)) {
        prep_log("prepare: no user folder (APPDATA unset); skipping the movie decoder\n");
        return;
    }
    wchar_t dir[MAX_PATH], user[MAX_PATH], beside[MAX_PATH], stamp[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, pref, -1, dir, MAX_PATH))
        return;
    swprintf(user, MAX_PATH, L"%lsffmpeg.exe", dir);
    swprintf(stamp, MAX_PATH, L"%lsffmpeg.download", dir);
    DWORD n = GetModuleFileNameW(NULL, beside, MAX_PATH);
    wchar_t *cut = n && n < MAX_PATH ? wcsrchr(beside, L'\\') : NULL;
    if (cut)
        wcscpy(cut + 1, L"ffmpeg.exe");
    if (GetFileAttributesW(user) != INVALID_FILE_ATTRIBUTES) {
        prep_log("prepare: ffmpeg present in the user folder -- nothing to do\n");
        return;
    }
    if (cut && GetFileAttributesW(beside) != INVALID_FILE_ATTRIBUTES) {
        prep_make_dirs(dir);
        int adopted = MoveFileW(beside, user);
        if (!adopted && CopyFileW(beside, user, FALSE))
            DeleteFileW(beside);
        if (adopted || GetFileAttributesW(user) != INVALID_FILE_ATTRIBUTES) {
            prep_log("prepare: adopted ffmpeg from beside the exe into %ls\n", user);
            return;
        }
        prep_log("prepare: ffmpeg sits beside the exe but could not be adopted\n");
    }
    {
        /* the movie player's other places (movie.c): the build setting, then PATH */
        wchar_t found[MAX_PATH], *part;
#ifdef MOON_FFMPEG
        if (GetFileAttributesA(MOON_FFMPEG) != INVALID_FILE_ATTRIBUTES) {
            prep_log("prepare: ffmpeg set by the build (%s) -- nothing to do\n", MOON_FFMPEG);
            return;
        }
#endif
        if (SearchPathW(NULL, L"ffmpeg.exe", NULL, MAX_PATH, found, &part)) {
            prep_log("prepare: ffmpeg on PATH (%ls) -- nothing to do\n", found);
            return;
        }
    }
    if (prep_stamp_fresh(stamp)) {
        prep_log("prepare: ffmpeg missing, but a download was tried less than a day ago; skipping\n");
        return;
    }
    const char *override = getenv("MOON_FFMPEG_URL");   /* dev/harness: exercise the failure path */
    wchar_t *url = (wchar_t *)FFMPEG_URL_DEFAULT;
    wchar_t env_url[4096];
    if (override && !strncmp(override, "https://", 8) &&
        MultiByteToWideChar(CP_UTF8, 0, override, -1, env_url, 4096)) {
        url = env_url;
        prep_log("prepare: using MOON_FFMPEG_URL override\n");
    }
    prep_job j;
    memset(&j, 0, sizeof j);
    wcsncpy(j.url, url, 4095);
    j.url[4095] = 0;
    wcsncpy(j.user, user, MAX_PATH - 1);
    wchar_t temp[MAX_PATH], unique[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, temp) || !GetTempFileNameW(temp, L"moe", 0, unique)) {
        prep_log("prepare: no temporary folder; skipping the download\n");
        return;
    }
    DeleteFileW(unique);
    if (!CreateDirectoryW(unique, NULL)) {
        prep_log("prepare: could not make a temporary folder; skipping the download\n");
        return;
    }
    wcsncpy(j.workdir, unique, MAX_PATH - 1);
    prep_log("prepare: ffmpeg missing; fetching BtbN FFmpeg-Builds (latest win64-gpl)\n");
    const char *shot = getenv("MOON_PREPARE_SHOT");
    HANDLE thread = CreateThread(NULL, 0, prep_worker, &j, 0, NULL);
    if (!thread) {
        prep_log("prepare: could not start the download thread\n");
        prep_recursive_delete(j.workdir);
        return;
    }
    prep_show(&j, shot && *shot ? shot : NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    LONG state = prep_state(&j);
    if (state == ST_DONE) {
        prep_log("prepare: the movie decoder is ready\n");
        DeleteFileW(stamp);
    } else {
        prep_log("prepare: %s; the game continues without the cutscenes\n",
                 state == ST_CANCELLED ? "the download was cancelled" : "the download did not finish");
        prep_log("prepare: it will not be retried for a day\n");
        prep_stamp_write(stamp);
    }
    prep_recursive_delete(j.workdir);
}
#else
void launcher_prepare_ffmpeg(void)
{
    /* The movie decoder is only spawned on Windows; nothing to prepare elsewhere. */
}
#endif
