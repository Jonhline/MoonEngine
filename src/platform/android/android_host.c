#include "android_host.h"
#include "android_state.h"
#include "dev_overlay.h"
#include "pad_config.h"
#include "platform/platform.h"
#include "video_gl.h"
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ---- the user root, migration, the log and the crash report ----
   Everything the app writes (launcher.ini, video.ini, options.ini, saves/,
   mods/, mods.ini, studio-rig.txt, moonengine.log) lives under ONE root. On
   Android that root is the app's EXTERNAL files dir -- /storage/emulated/0/
   Android/data/org.moonengine.android/files/MoonEngine/MoonEngine/ -- because
   that is the one place the captain can reach over MTP/SAF and sync out with
   Syncthing (he already syncs PS2 memcards out of an /Android/data/... folder).
   The private internal dir (/data/data/.../files) is unreachable by any file
   manager, which is exactly what was wrong. The layout under the root is the
   same one desktop uses, so the paths read the same everywhere. */
#define MOON_ANDROID_SUBDIR "MoonEngine/MoonEngine"
#define MOON_LOG_ROTATE (2u * 1024u * 1024u)
/* The Download mirror GameActivity keeps in step with the log (its own copy of
   this path is in GameActivity.startLogMirror). */
#define MOON_ANDROID_LOG_MIRROR "/storage/emulated/0/Download/MoonEngine/moonengine.log"

static int moon_is_dir(const char *path)
{
    struct stat info;
    return path && *path && !stat(path, &info) && S_ISDIR(info.st_mode);
}

/* Create every component of a directory path. Idempotent; a failure prints one
   line and never stops the app (the game carries on, as before). */
static void moon_make_dirs(const char *path)
{
    if (!path || !*path)
        return;
    char tmp[1300];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *at = tmp + 1; *at; at++)
        if (*at == '/') {
            *at = 0;
            mkdir(tmp, 0755);
            *at = '/';
        }
    mkdir(tmp, 0755);
}

static unsigned moon_count_files(const char *dir, const char *skip)
{
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    unsigned count = 0;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") ||
            (skip && !strcmp(entry->d_name, skip)))
            continue;
        char path[1300];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        struct stat info;
        if (lstat(path, &info))
            continue;
        if (S_ISDIR(info.st_mode))
            count += moon_count_files(path, NULL);
        else if (S_ISREG(info.st_mode))
            count++;
    }
    closedir(d);
    return count;
}

static uint64_t moon_checksum(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    uint64_t hash = 1469598103934665603ull;
    char buffer[65536];
    size_t got;
    while ((got = fread(buffer, 1, sizeof buffer, f)) > 0)
        for (size_t i = 0; i < got; i++) {
            hash ^= (unsigned char)buffer[i];
            hash *= 1099511628211ull;
        }
    if (ferror(f))
        hash = 0;
    fclose(f);
    return hash ? hash : 1;
}

struct moon_copy_stats {
    unsigned files;
    uint64_t bytes;
    char sample_from[2][1300], sample_to[2][1300];
    unsigned samples;
};

static int moon_copy_file(const char *from, const char *to, struct moon_copy_stats *stats)
{
    FILE *in = fopen(from, "rb");
    if (!in)
        return 0;
    FILE *out = fopen(to, "wb");
    if (!out) {
        fclose(in);
        return 0;
    }
    char buffer[65536];
    size_t got;
    int ok = 1;
    while (ok && (got = fread(buffer, 1, sizeof buffer, in)) > 0) {
        if (fwrite(buffer, 1, got, out) != got)
            ok = 0;
        else
            stats->bytes += got;
    }
    if (ferror(in))
        ok = 0;
    fclose(in);
    if (fclose(out))
        ok = 0;
    if (ok && stats->samples < 2) {
        snprintf(stats->sample_from[stats->samples], 1300, "%s", from);
        snprintf(stats->sample_to[stats->samples], 1300, "%s", to);
        stats->samples++;
    }
    return ok;
}

/* Copy `from` into `to`, skipping one top-level child by name. A path that does
   not exist copies nothing and succeeds ("nothing to adopt"). */
static int moon_copy_tree(const char *from, const char *to, const char *skip,
                          struct moon_copy_stats *stats)
{
    if (!moon_is_dir(from))
        return 1;
    DIR *d = opendir(from);
    if (!d)
        return 0;
    int ok = 1;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") ||
            (skip && !strcmp(entry->d_name, skip)))
            continue;
        char a[1300], b[1300];
        snprintf(a, sizeof a, "%s/%s", from, entry->d_name);
        snprintf(b, sizeof b, "%s/%s", to, entry->d_name);
        struct stat info;
        if (lstat(a, &info))
            continue;
        if (S_ISDIR(info.st_mode)) {
            if (!moon_is_dir(b))
                mkdir(b, 0755);
            ok &= moon_copy_tree(a, b, NULL, stats);
        } else if (S_ISREG(info.st_mode)) {
            if (moon_copy_file(a, b, stats))
                stats->files++;
            else
                ok = 0;
        }
    }
    closedir(d);
    return ok;
}

/* Adopt the old private root ONCE: if the new external root holds no user files
   and the private one does, COPY (never move) the whole tree across -- both the
   documented <private>/MoonEngine/MoonEngine/ root and anything left loose in
   the SDL pref dir (<private>/) such as launcher.ini and saves/. Verify the
   copy (file count + a checksum of up to two files); the old files are always
   left in place, so a failed copy can never lose a save. */
static void moon_android_adopt_user_root(const char *internal, const char *root)
{
    if (!internal || !*internal || !root || !*root || !strcmp(internal, root))
        return;
    if (moon_count_files(root, NULL) > 0)
        return; /* already adopted, or the captain already has data here */
    char old_root[1300];
    snprintf(old_root, sizeof old_root, "%s/%s", internal, MOON_ANDROID_SUBDIR);
    unsigned expected = moon_count_files(old_root, NULL) + moon_count_files(internal, "MoonEngine");
    if (!expected)
        return; /* nothing written yet: a fresh install, nothing to carry over */

    struct moon_copy_stats stats = {0};
    int ok = moon_copy_tree(old_root, root, NULL, &stats);
    ok &= moon_copy_tree(internal, root, "MoonEngine", &stats);
    int verified = ok && stats.files == expected && moon_count_files(root, NULL) >= stats.files;
    for (unsigned i = 0; verified && i < stats.samples; i++) {
        uint64_t from = moon_checksum(stats.sample_from[i]);
        uint64_t to = moon_checksum(stats.sample_to[i]);
        if (!from || from != to)
            verified = 0;
    }
    if (verified)
        fprintf(stderr,
                "player data: adopted %u file(s) (%llu bytes) from %s into %s; the old copy is left "
                "in place\n",
                stats.files, (unsigned long long)stats.bytes, old_root, root);
    else
        fprintf(stderr,
                "player data: copy from %s to %s could NOT be verified; the old files are untouched "
                "(still private), leaving the old root usable\n",
                old_root, root);
}

/* ---- the log, and a crash report the captain can attach ----
   The log is appended (never truncated) so a crash line survives the next
   launch; it is rotated only past MOON_LOG_ROTATE. g_log_fd is kept for the
   fatal handler, which writes one final, async-signal-safe line and fsyncs it. */
static int g_log_fd = -1;
static const char *volatile g_last_frame;
/* The inventory line main.c prints at startup asks for these two. */
static char g_log_path[1300];
static char g_log_mirror[1300];

const char *moon_android_log_path(void)
{
    return g_log_path[0] ? g_log_path : NULL;
}

const char *moon_android_log_mirror(void)
{
    return g_log_mirror[0] ? g_log_mirror : NULL;
}

void moon_android_mark_frame(const char *where)
{
    g_last_frame = where;
}

static void fatal_put(const char *text)
{
    if (g_log_fd < 0)
        return;
    while (*text) {
        ssize_t wrote = write(g_log_fd, text, 1);
        if (wrote < 0 && errno == EINTR)
            continue;
        if (wrote <= 0)
            return;
        text++;
    }
}

static void fatal_int(int value)
{
    char digits[12];
    int n = 0;
    unsigned number = value < 0 ? (unsigned)-value : (unsigned)value;
    if (value < 0)
        fatal_put("-");
    if (!number)
        digits[n++] = '0';
    while (number) {
        digits[n++] = (char)('0' + number % 10);
        number /= 10;
    }
    while (n)
        fatal_put((char[]){digits[--n], 0});
}

static void moon_fatal_signal(int signo)
{
    fatal_put("fatal: signal ");
    fatal_int(signo);
    fatal_put(", last frame ");
    const char *where = g_last_frame;
    if (where)
        fatal_put(where);
    else
        fatal_put("(unknown)");
    fatal_put("\n");
    if (g_log_fd >= 0)
        fsync(g_log_fd);
    signal(signo, SIG_DFL);
    raise(signo);
}

static void moon_install_fatal_handlers(void)
{
    static const int signals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGFPE};
    for (unsigned i = 0; i < sizeof signals / sizeof *signals; i++) {
        struct sigaction action;
        memset(&action, 0, sizeof action);
        action.sa_handler = moon_fatal_signal;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESETHAND | SA_NODEFER;
        sigaction(signals[i], &action, NULL);
    }
}

/* G6.40: GameActivity's volume-up + volume-down chord lands here (the JNI
   symbol is resolved by name when libmain is loaded). SDL never sees the
   volume keys, so this is the one hook that flips the debug overlay. */
JNIEXPORT void JNICALL Java_org_moonengine_android_GameActivity_nativeToggleOverlay(
    JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    moon_debug_overlay_toggle();
}
/* Ask the Java document picker for the player's own disc. The launcher calls this from
   SDL's main thread; the Activity lifecycle runs on Android's UI thread, so wait here
   and let the picker finish. The descriptor stays in this process (same process as the
   launcher and the game), and native reads it through /proc/self/fd/N exactly as
   GameActivity's own startup does. */
int plat_pick_disc(char *out, size_t cap)
{
    if (!out || !cap)
        return 0;
    out[0] = 0;
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity)
        return 0;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID start = cls ? (*env)->GetMethodID(env, cls, "startDiscPicker", "()V") : NULL;
    jmethodID result =
        cls ? (*env)->GetMethodID(env, cls, "discPickerResult", "()Ljava/lang/String;") : NULL;
    int picked = 0;
    if (start && result) {
        (*env)->CallVoidMethod(env, activity, start);
        if ((*env)->ExceptionCheck(env))
            (*env)->ExceptionClear(env);
        else
            /* null = still open; "" = cancelled; a path = chosen. */
            for (unsigned waited = 0; waited < 600000u; waited += 50) {
                jstring found = (jstring)(*env)->CallObjectMethod(env, activity, result);
                if ((*env)->ExceptionCheck(env)) {
                    (*env)->ExceptionClear(env);
                    break;
                }
                if (found) {
                    const char *path = (*env)->GetStringUTFChars(env, found, NULL);
                    if (path) {
                        if (path[0]) {
                            snprintf(out, cap, "%s", path);
                            picked = 1;
                        }
                        (*env)->ReleaseStringUTFChars(env, found, path);
                    }
                    (*env)->DeleteLocalRef(env, found);
                    break;
                }
                SDL_Delay(50);
            }
    }
    if (cls)
        (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);
    return picked;
}
void moon_android_init(void)
{
    const char *internal = SDL_AndroidGetInternalStoragePath();
    const char *external = SDL_AndroidGetExternalStoragePath();
    const char *base = external ? external : internal; /* external storage may be unmounted */
    char root[1200] = "";
    if (base) {
        snprintf(root, sizeof root, "%s/%s", base, MOON_ANDROID_SUBDIR);
        if (external) {
            moon_make_dirs(root);
            moon_android_adopt_user_root(internal, root);
        } else
            fprintf(stderr,
                    "moonengine: external storage unavailable; user data stays in private storage "
                    "%s\n",
                    internal ? internal : "(none)");
        SDL_setenv("XDG_DATA_HOME", base, 1);
        snprintf(g_log_path, sizeof g_log_path, "%s/moonengine.log", root);
        snprintf(g_log_mirror, sizeof g_log_mirror, "%s", MOON_ANDROID_LOG_MIRROR);
        char logpath[1300];
        snprintf(logpath, sizeof logpath, "%s", g_log_path);
        struct stat info;
        if (!stat(logpath, &info) && info.st_size > (off_t)MOON_LOG_ROTATE) {
            char previous[1300];
            snprintf(previous, sizeof previous, "%s.prev", logpath);
            rename(logpath, previous);
        }
        if (freopen(logpath, "a", stderr)) {
            setvbuf(stderr, NULL, _IOLBF, 8192);
            dup2(fileno(stderr), fileno(stdout));
            setvbuf(stdout, NULL, _IOLBF, 8192);
            g_log_fd = fileno(stderr);
        }
        moon_install_fatal_handlers();
        /* The whole user-data inventory (root, every file, log and mirror) is
           printed once by main.c -- one line, same shape on every platform. */
        fprintf(stderr, "moonengine: --- session start ---\n");
    }
    SDL_setenv("MOON_GRAPHICS_API", "vulkan", 1);
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
}
void moon_android_configure(const moon_mobile_options *options, int running)
{
    moon_mobile_controls *s = moon_android_controls();
    int connected = plat_sdl_pad(0) != NULL;
    if (s->active != running || s->controller != connected ||
        memcmp(&s->options, options, sizeof *options))
        moon_mobile_reset(s);
    s->active = running;
    s->controller = connected;
    s->options = *options;
}
int moon_android_event(plat_key_event *event)
{
    moon_mobile_controls *s = moon_android_controls();
    if (!event->touch)
        return 0;
    plat_pad_state before, after;
    moon_mobile_pad(s, &before);
    int consumed =
        moon_mobile_contact(s, event->touch, event->touch_id, event->touch_x, event->touch_y);
    moon_mobile_pad(s, &after);
    if ((after.buttons & ~before.buttons) & (1u << GT4_PAD_START)) {
        event->touch = 0;
        event->key = SDLK_ESCAPE;
        return 0;
    }
    return consumed;
}
void moon_android_overlay(ui_project *ui)
{
    moon_mobile_controls *s = moon_android_controls();
    if (!s->active || !s->options.enabled || s->controller)
        return;
    static const char *labels[MOON_MOBILE_COUNT] = {"STEER", "GAS",  "BRAKE", "+",  "-",
                                                    "HAND",  "VIEW", "PAUSE", "REV"};
    plat_video_set_view(640, 480);
    for (int c = 0; c < MOON_MOBILE_COUNT; c++) {
        moon_mobile_rect r = moon_mobile_bounds(s, c);
        float x = r.x * 640, y = r.y * 480, w = r.w * 640, h = r.h * 480;
        int pressed = 0;
        for (int i = 0; i < 10; i++)
            pressed |= s->contacts[i].used && s->contacts[i].control == c;
        uint8_t ink[] = {255, 255, 255, (uint8_t)((s->options.opacity + 1) * 51)};
        uint8_t bg[] = {20, 22, 26, (uint8_t)(ink[3] * (pressed ? .65f : .35f))};
        plat_draw_quad(NULL, x, y, w, h, 0, 0, 1, 1, bg);
        plat_draw_quad(NULL, x, y, w, 1, 0, 0, 1, 1, ink);
        plat_draw_quad(NULL, x, y + h - 1, w, 1, 0, 0, 1, 1, ink);
        plat_draw_quad(NULL, x, y, 1, h, 0, 0, 1, 1, ink);
        plat_draw_quad(NULL, x + w - 1, y, 1, h, 0, 0, 1, 1, ink);
        ui_draw_string(ui, NULL, x + 5, y + h * .45f, .65f, 0, ink, labels[c]);
    }
}
