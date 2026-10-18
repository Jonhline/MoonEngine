#include "android_host.h"
#include "android_state.h"
#include "dev_overlay.h"
#include "pad_config.h"
#include "platform/platform.h"
#include "video_gl.h"
#include <SDL.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
    const char *dir = SDL_AndroidGetInternalStoragePath();
    if (dir) {
        SDL_setenv("XDG_DATA_HOME", dir, 1);
        char path[1200];
        snprintf(path, sizeof path, "%s/moonengine.log", dir);
        if (freopen(path, "w", stderr)) {
            setvbuf(stderr, NULL, _IOLBF, 8192);
            dup2(fileno(stderr), fileno(stdout));
            setvbuf(stdout, NULL, _IOLBF, 8192);
        }
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
