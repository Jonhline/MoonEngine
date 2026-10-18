/*
 * SDL2 backend: time, input events, dialogs and audio. Video (window,
 * OpenGL 2D and 3D) is in video_gl.c.
 */
#include "platform/switch_profile.h"
#include "platform/platform.h"
#include "plat_sdl.h"
#ifdef __ANDROID__
#include "video_gl.h"
#endif

#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(__SWITCH__) && defined(MOON_DEKO3D)
#include <switch.h>
#include "../switch/touch_contact.h"
/* The native renderer owns NWindow, so contacts cannot depend on SDL focus. */
static int native_touch(plat_key_event *ev)
{
    static int initialized;
    static moon_touch_contact contact;
    if (!initialized) {
        hidInitializeTouchScreen();
        contact.mode = appletGetOperationMode() == AppletOperationMode_Handheld;
        initialized = 1;
    }
    HidTouchScreenState state = {0};
    if (!hidGetTouchScreenStates(&state, 1))
        return 0;
    moon_touch_point points[16];
    int count = state.count < 0 ? 0 : state.count > 16 ? 16 : state.count;
    for (int i = 0; i < count; i++) {
        const HidTouchState *p = &state.touches[i];
        points[i] =
            (moon_touch_point){p->finger_id, p->x, p->y, !!(p->attributes & HidTouchAttribute_End)};
    }
    return moon_touch_sample(&contact, points, count,
                             appletGetOperationMode() == AppletOperationMode_Handheld,
                             appletGetFocusState() == AppletFocusState_InFocus, ev);
}
#endif

static int quit;
static int confine_mouse;
int plat_sdl_mouse_should_confine(int enabled, unsigned flags)
{
    return enabled && (flags & SDL_WINDOW_FULLSCREEN) && (flags & SDL_WINDOW_INPUT_FOCUS) &&
           !(flags & SDL_WINDOW_MINIMIZED);
}
void plat_sdl_confine_mouse(int enabled)
{
#ifndef __SWITCH__
    confine_mouse = enabled != 0;
    SDL_Window *window = plat_sdl_window();
    if (!window)
        return;
    SDL_bool grab = plat_sdl_mouse_should_confine(confine_mouse, SDL_GetWindowFlags(window))
                        ? SDL_TRUE
                        : SDL_FALSE;
    if (SDL_GetWindowMouseGrab(window) != grab)
        SDL_SetWindowMouseGrab(window, grab);
#else
    (void)enabled;
#endif
}
static unsigned music_fade_left, music_fade_total;
/* Playback must not repeatedly reopen a failed endpoint on the UI thread.
   A device-added event or explicit initialization can retry it. */
static int audio_unavailable;
static Uint32 audio_failed_at;
static int dialog_answer = -1;

void plat_sdl_dialog_answer(int answer)
{
    dialog_answer = answer;
}
int plat_sdl_get_dialog_answer(void)
{
    return dialog_answer;
}

int plat_dialog(const char *title, const char *message, int confirm)
{
    if (dialog_answer >= 0)
        return confirm ? dialog_answer : 1;
    /* In-game confirmations require the app presenter; never open OS UI. */
    (void)title;
    (void)message;
    return -1;
}

uint64_t plat_time_ns(void)
{
    return (uint64_t)((double)SDL_GetPerformanceCounter() * 1e9 /
                      (double)SDL_GetPerformanceFrequency());
}

void plat_sleep_ms(uint32_t ms)
{
    SDL_Delay(ms);
}

int plat_should_quit(void)
{
    return quit;
}

int plat_sdl_poll(plat_key_event *ev)
{
    plat_sdl_confine_mouse(confine_mouse);
    SDL_Event e;
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    static SDL_Keycode fullscreen_enter;
#endif
    memset(ev, 0, sizeof *ev);
    plat_sdl_pad(0);
#if defined(__SWITCH__) && defined(MOON_DEKO3D)
    if (native_touch(ev))
        return 1;
#endif
    while (SDL_PollEvent(&e)) {
#if !defined(__SWITCH__) && !defined(__ANDROID__)
        if (fullscreen_enter && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) &&
            e.key.keysym.sym == fullscreen_enter) {
            if (e.type == SDL_KEYUP)
                fullscreen_enter = 0;
            continue;
        }
#endif
#if !defined(__SWITCH__) && !defined(__ANDROID__)
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            fullscreen_enter = 0;
#endif
        if (e.type == SDL_QUIT) {
            quit = 1;
        } else if (e.type == SDL_AUDIODEVICEADDED && !e.adevice.iscapture) {
            if (audio_unavailable && (Sint32)(e.common.timestamp - audio_failed_at) > 0)
                audio_unavailable = 0;
        } else if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION ||
                   e.type == SDL_FINGERUP) {
#if defined(__SWITCH__) && defined(MOON_DEKO3D)
            continue; /* libnx supplies the single authoritative contact stream. */
#endif
            ev->touch = e.type == SDL_FINGERDOWN ? 1 : e.type == SDL_FINGERMOTION ? 2 : 3;
            ev->touch_id = e.tfinger.fingerId;
            ev->touch_x = e.tfinger.x;
            ev->touch_y = e.tfinger.y;
#ifdef __ANDROID__
            int width = 0, height = 0;
            SDL_GetWindowSize(plat_sdl_window(), &width, &height);
            float x = -1, y = -1;
            video_window_to_ui((int)(e.tfinger.x * width), (int)(e.tfinger.y * height), &x, &y);
            ev->touch_x = x / 640.f;
            ev->touch_y = y / video_view_height();
#endif
            return 1;
#ifdef __ANDROID__
        } else if (e.type == SDL_APP_WILLENTERBACKGROUND || e.type == SDL_APP_DIDENTERFOREGROUND) {
            ev->lifecycle = e.type == SDL_APP_WILLENTERBACKGROUND ? 1 : 2;
            ev->touch = 4;
            return 1;
#endif
        } else if ((e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) ||
                   e.type == SDL_APP_WILLENTERBACKGROUND || e.type == SDL_DISPLAYEVENT) {
            ev->touch = 4;
            return 1;
        } else if ((e.type == SDL_MOUSEMOTION && e.motion.which == SDL_TOUCH_MOUSEID) ||
                   ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) &&
                    e.button.which == SDL_TOUCH_MOUSEID)) {
            continue; /* A contact is routed once, never again as a synthetic mouse click. */
        } else if (e.type == SDL_MOUSEMOTION && SDL_GetRelativeMouseMode()) {
            ev->mouse = 1;
            ev->mouse_x = e.motion.xrel;
            ev->mouse_y = e.motion.yrel;
            return 1;
        } else if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONUP ||
                   e.type == SDL_MOUSEBUTTONDOWN) {
            ev->mouse = ev->mouse_absolute = 1;
            if (e.type == SDL_MOUSEMOTION) {
                ev->mouse_x = e.motion.x;
                ev->mouse_y = e.motion.y;
                if (e.motion.state & SDL_BUTTON_LMASK)
                    ev->mouse_button = 4;
            } else {
                ev->mouse_x = e.button.x;
                ev->mouse_y = e.button.y;
                ev->mouse_button = e.button.button == SDL_BUTTON_LEFT    ? 1
                                   : e.button.button == SDL_BUTTON_RIGHT ? 2
                                                                         : 0;
                if (e.type == SDL_MOUSEBUTTONDOWN)
                    ev->mouse_button = e.button.button == SDL_BUTTON_LEFT ? 3 : 0;
            }
            return 1;
        } else if (e.type == SDL_MOUSEWHEEL) {
            ev->mouse = 1;
            ev->mouse_absolute = !SDL_GetRelativeMouseMode();
            if (ev->mouse_absolute)
                SDL_GetMouseState(&ev->mouse_x, &ev->mouse_y);
            ev->wheel = e.wheel.y * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1);
            return 1;
        } else if (e.type == SDL_TEXTINPUT) {
            ev->key = 0;
            snprintf(ev->text, sizeof ev->text, "%s", e.text.text);
            return 1;
#if !defined(__SWITCH__) && !defined(__ANDROID__)
        } else if (e.type == SDL_KEYDOWN &&
                   (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) &&
                   (e.key.keysym.mod & KMOD_ALT)) {
            fullscreen_enter = e.key.keysym.sym;
            if (!e.key.repeat) {
                SDL_Window *window = SDL_GetWindowFromID(e.key.windowID);
                if (window) {
                    Uint32 mode = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN
                                      ? 0
                                      : SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(window, mode);
                }
            }
            /* Consume the shortcut: Enter must not also activate a menu item. */
            continue;
#endif
        } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            ev->key = e.key.keysym.sym;
            ev->ps2key = plat_sdl_ps2_key(&e, &ev->release);
            return 1;
        } else if (e.type == SDL_KEYUP) {
            ev->ps2key = plat_sdl_ps2_key(&e, &ev->release);
            if (ev->ps2key) {
                ev->key = 0;
                return 1;
            }
        } else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP ||
                   e.type == SDL_CONTROLLERAXISMOTION) {
            ev->ps2key = plat_sdl_ps2_key(&e, &ev->release);
            ev->key = e.type == SDL_CONTROLLERBUTTONUP ? 0 : plat_sdl_controller_key(&e);
            if (ev->key || ev->ps2key) {
                ev->pad = 1;
#ifndef __SWITCH__
                SDL_JoystickID id =
                    e.type == SDL_CONTROLLERAXISMOTION ? e.caxis.which : e.cbutton.which;
                SDL_GameController *controller = SDL_GameControllerFromInstanceID(id);
                SDL_GameControllerType type = controller ? SDL_GameControllerGetType(controller)
                                                         : SDL_CONTROLLER_TYPE_UNKNOWN;
                ev->pad_type =
                    type == SDL_CONTROLLER_TYPE_XBOX360 || type == SDL_CONTROLLER_TYPE_XBOXONE ? 2
                                                                                               : 0;
#endif
                return 1;
            }
        }
    }
    return 0;
}

/* PCM voices are copied on submission, mixed on SDL's audio thread. */
static SDL_AudioDeviceID audio_device;
/* P: opt-in recording of this process's final PCM mix, excluding other
   desktop applications. First uint64 is the first block's monotonic ns;
   the remainder is 48000 Hz stereo S16. Disabled in ordinary playback. */
static FILE *audio_capture;
static int audio_capture_started;
static struct {
    int16_t *pcm;
    size_t frames, pos;
} voices[16];
static plat_audio_cb audio_callback;
static void *audio_user;
/* Single producer / single consumer. Music synthesis never runs in the
   device callback; 170 ms of headroom absorbs scheduler/driver stalls. */
enum { MUSIC_RING = 16384, MUSIC_AHEAD = PLAT_SDL_MUSIC_PREROLL, MUSIC_BLOCK = 512 };
static int16_t music_ring[MUSIC_RING * 2];
static SDL_atomic_t music_read, music_write, music_stop, music_underruns, music_played;
/* The last block the device took, under the device lock. */
static int music_block_start, music_block_frames;
static Uint64 music_block_stamp;
static SDL_Thread *music_thread;
static void music_produce(void)
{
    int16_t block[MUSIC_BLOCK * 2];
    int r = SDL_AtomicGet(&music_read), w = SDL_AtomicGet(&music_write);
    int queued = (w - r) & (MUSIC_RING - 1);
    if (queued >= MUSIC_AHEAD)
        return;
    SWP_VOID(SWP_AUDIO_SYNTH, audio_callback(block, MUSIC_BLOCK, audio_user));
    for (int i = 0; i < MUSIC_BLOCK; i++) {
        int at = (w + i) & (MUSIC_RING - 1);
        music_ring[at * 2] = block[i * 2];
        music_ring[at * 2 + 1] = block[i * 2 + 1];
    }
    SDL_AtomicSet(&music_write, (w + MUSIC_BLOCK) & (MUSIC_RING - 1));
}
static int music_worker(void *unused)
{
    (void)unused;
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
    while (!SDL_AtomicGet(&music_stop)) {
        int queued = (SDL_AtomicGet(&music_write) - SDL_AtomicGet(&music_read)) & (MUSIC_RING - 1);
        if (queued < MUSIC_AHEAD)
            music_produce();
        else
            SDL_Delay(1);
    }
    return 0;
}

static void audio_mix(void *unused, Uint8 *stream, int bytes)
{
    (void)unused;
    SWP_PUSH(SWP_AUDIO_MIX);
    int16_t *out = (int16_t *)stream;
    int frames = bytes / 4;
    SDL_memset(stream, 0, (size_t)bytes);
    if (audio_callback) {
        int r = SDL_AtomicGet(&music_read), w = SDL_AtomicGet(&music_write);
        int available = (w - r) & (MUSIC_RING - 1), take = frames < available ? frames : available;
        for (int i = 0; i < take; i++) {
            int at = (r + i) & (MUSIC_RING - 1);
            out[i * 2] = music_ring[at * 2];
            out[i * 2 + 1] = music_ring[at * 2 + 1];
        }
        SDL_AtomicSet(&music_read, (r + take) & (MUSIC_RING - 1));
        /* The device plays this block over the next frames/48000 s. */
        music_block_start = SDL_AtomicGet(&music_played);
        music_block_frames = take;
        music_block_stamp = SDL_GetPerformanceCounter();
        SDL_AtomicAdd(&music_played, take);
        if (take < frames)
            SDL_AtomicAdd(&music_underruns, 1);
    }
    for (int f = 0; f < frames; f++) {
        int l = out[f * 2], r = out[f * 2 + 1];
        if (music_fade_total) {
            l = (int)((int64_t)l * music_fade_left / music_fade_total);
            r = (int)((int64_t)r * music_fade_left / music_fade_total);
            if (music_fade_left)
                music_fade_left--;
        }
        for (int v = 0; v < 16; v++)
            if (voices[v].pcm && voices[v].pos < voices[v].frames) {
                size_t p = voices[v].pos++ * 2;
                l += voices[v].pcm[p];
                r += voices[v].pcm[p + 1];
            }
        out[f * 2] = (int16_t)(l < -32768 ? -32768 : l > 32767 ? 32767 : l);
        out[f * 2 + 1] = (int16_t)(r < -32768 ? -32768 : r > 32767 ? 32767 : r);
    }
    if (audio_capture) {
        if (!audio_capture_started) {
            uint64_t stamp = plat_time_ns();
            fwrite(&stamp, sizeof stamp, 1, audio_capture);
            audio_capture_started = 1;
        }
        fwrite(stream, 1, (size_t)bytes, audio_capture);
    }
    SWP_POP();
}
int plat_audio_init(const plat_audio_desc *desc, plat_audio_cb cb, void *user)
{
    if (audio_device)
        return 0;
    if (desc->sample_rate != 48000 || desc->channels != 2)
        return -1;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        audio_unavailable = 1;
        audio_failed_at = SDL_GetTicks();
        return -1;
    }
    if (SDL_GetNumAudioDevices(0) == 0) {
        audio_unavailable = 1;
        audio_failed_at = SDL_GetTicks();
        SDL_Log("Audio unavailable: no playback device; continuing without audio");
        return -1;
    }
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = 48000;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 1024;
    spec.callback = audio_mix;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (!audio_device) {
        audio_unavailable = 1;
        audio_failed_at = SDL_GetTicks();
        SDL_Log("Audio unavailable: %s", SDL_GetError());
        return -1;
    }
    audio_unavailable = 0;
    audio_callback = cb;
    audio_user = user;
    SDL_AtomicSet(&music_read, 0);
    SDL_AtomicSet(&music_write, 0);
    SDL_AtomicSet(&music_stop, 0);
    SDL_AtomicSet(&music_underruns, 0);
    if (cb) {
        for (int i = 0; i < MUSIC_AHEAD / MUSIC_BLOCK; i++)
            music_produce();
        music_thread = SDL_CreateThread(music_worker, "Moon Engine music", NULL);
        if (!music_thread) {
            SDL_CloseAudioDevice(audio_device);
            audio_device = 0;
            audio_callback = NULL;
            audio_user = NULL;
            return -1;
        }
    }
    {
        const char *path = getenv("MOON_CAPTURE_AUDIO");
        if (path && *path)
            audio_capture = fopen(path, "wb");
        audio_capture_started = 0;
    }
    SDL_PauseAudioDevice(audio_device, 0);
    return 0;
}
int plat_audio_set_music(plat_audio_cb cb, void *user)
{
    plat_audio_desc desc = {48000, 2};
    if (!audio_device && (audio_unavailable || plat_audio_init(&desc, NULL, NULL)))
        return -1;
    SDL_AtomicSet(&music_stop, 1);
    if (music_thread) {
        SDL_WaitThread(music_thread, NULL);
        music_thread = NULL;
    }
    SDL_LockAudioDevice(audio_device);
    music_fade_left = music_fade_total = 0;
    audio_callback = cb;
    audio_user = user;
    SDL_AtomicSet(&music_read, 0);
    SDL_AtomicSet(&music_write, 0);
    SDL_AtomicSet(&music_stop, 0);
    SDL_AtomicSet(&music_played, 0);
    music_block_start = music_block_frames = 0;
    music_block_stamp = 0;
    int result = 0;
    if (cb) {
        for (int i = 0; i < MUSIC_AHEAD / MUSIC_BLOCK; i++)
            music_produce();
        music_thread = SDL_CreateThread(music_worker, "Moon Engine music", NULL);
        if (!music_thread) {
            audio_callback = NULL;
            audio_user = NULL;
            result = -1;
        }
    }
    SDL_UnlockAudioDevice(audio_device);
    return result;
}
int plat_audio_music_owned(void *user)
{
    return audio_device && audio_callback && audio_user == user;
}
void plat_audio_release_music(void *user)
{
    if (audio_device && audio_callback && audio_user == user)
        plat_audio_set_music(NULL, NULL);
}
uint64_t plat_audio_music_position(void)
{
    /* The device takes 1024-frame blocks (21 ms); counting only whole blocks
       makes a clock that jumps, and pictures following it skip frames.
       Advance through the current block by elapsed time instead. */
    if (!audio_device)
        return (uint64_t)(unsigned)SDL_AtomicGet(&music_played);
    SDL_LockAudioDevice(audio_device);
    int start = music_block_start, frames = music_block_frames;
    Uint64 stamp = music_block_stamp;
    SDL_UnlockAudioDevice(audio_device);
    if (!stamp)
        return (uint64_t)(unsigned)SDL_AtomicGet(&music_played);
    Uint64 into = (SDL_GetPerformanceCounter() - stamp) * 48000 / SDL_GetPerformanceFrequency();
    return (uint64_t)(unsigned)start + (into < (Uint64)frames ? into : (Uint64)frames);
}
int plat_audio_play(const int16_t *pcm, size_t frames, int rate, int channels)
{
    if (!pcm || !frames || frames > 48000 * 30 || rate != 48000 || channels != 2)
        return 0;
    plat_audio_desc desc = {48000, 2};
    if (!audio_device && (audio_unavailable || plat_audio_init(&desc, NULL, NULL) != 0))
        return 0;
    int16_t *copy = malloc(frames * 4);
    if (!copy)
        return 0;
    SDL_memcpy(copy, pcm, frames * 4);
    SDL_LockAudioDevice(audio_device);
    int slot = -1;
    for (int i = 0; i < 16; i++)
        if (!voices[i].pcm || voices[i].pos == voices[i].frames) {
            slot = i;
            break;
        }
    int16_t *old = NULL;
    if (slot >= 0) {
        old = voices[slot].pcm;
        voices[slot].pcm = copy;
        voices[slot].frames = frames;
        voices[slot].pos = 0;
    }
    SDL_UnlockAudioDevice(audio_device);
    free(old);
    if (slot < 0)
        free(copy);
    return slot >= 0;
}
void plat_audio_shutdown(void)
{
    SDL_AtomicSet(&music_stop, 1);
    if (music_thread) {
        SDL_WaitThread(music_thread, NULL);
        music_thread = NULL;
    }
    if (audio_device)
        SDL_CloseAudioDevice(audio_device);
    if (audio_capture) {
        fclose(audio_capture);
        audio_capture = NULL;
    }
    audio_capture_started = 0;
    audio_device = 0;
    audio_callback = NULL;
    audio_user = NULL;
    audio_unavailable = 0;
    for (int i = 0; i < 16; i++) {
        free(voices[i].pcm);
        SDL_memset(&voices[i], 0, sizeof voices[i]);
    }
}

unsigned plat_sdl_audio_underruns(void)
{
    return (unsigned)SDL_AtomicGet(&music_underruns);
}

void plat_audio_fade_music(float seconds)
{
    if (!audio_device)
        return;
    SDL_LockAudioDevice(audio_device);
    music_fade_total = music_fade_left =
        seconds > 0 && seconds < 60 ? (unsigned)(seconds * 48000) : 1;
    SDL_UnlockAudioDevice(audio_device);
}

int plat_sdl_music_fading(void)
{
    if (!audio_device)
        return 0;
    SDL_LockAudioDevice(audio_device);
    int fading = music_fade_left != 0;
    SDL_UnlockAudioDevice(audio_device);
    return fading;
}
