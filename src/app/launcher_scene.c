/* P: the launcher preview has an isolated renderer lifetime. Its hidden child
   uses the same models/shaders as the game; no render state survives Play.
   Only pixels and presentation selection cross the local shared mapping. */
#include "launcher_scene.h"
#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "save.h"
#include "gtmode_game.h"
#include "vol.h"
float launcher_scene_transition(float seconds)
{
    /* P: authored 1.5-second dissolve, with zero slope at both ends. */
    float phase = fmodf(fmaxf(0, seconds), LAUNCHER_SHOT_SECONDS);
    float t = fminf(1, phase / 1.5f);
    return t * t * (3 - 2 * t);
}
void launcher_scene_camera(float seconds, int fixed, float look[9], float *fov)
{
    /* P: authored PC presentation shots, not original game camera data.
       Each pair is a twelve-second dolly. Dissolves keep the camera outside the car
       when changing between opposite sides. No vehicle state is animated. */
    static const float shots[18][2][7] = {
        {{-4.8f, 1.35f, -5.6f, 0, .55f, 0, 32}, {-4.1f, 1.1f, -5.0f, 0, .55f, 0, 32}},
        {{-2.7f, .7f, -2.8f, -.72f, .4f, -1.25f, 30},
         {-2.05f, .55f, -2.35f, -.72f, .4f, -1.25f, 30}},
        {{-5.8f, 1.3f, -1.2f, 0, .6f, 0, 36}, {-5.5f, 1.1f, .7f, 0, .6f, 0, 36}},
        {{4.8f, 1.45f, 5.7f, 0, .65f, .3f, 32}, {4.0f, 1.15f, 5.0f, 0, .65f, .3f, 32}},
        {{.8f, 1.05f, 3.8f, 0, .7f, 1.6f, 30}, {-.3f, .85f, 3.4f, 0, .7f, 1.6f, 30}},
        {{-5.2f, 1.8f, -6.0f, 0, .55f, 0, 32}, {-4.8f, 1.35f, -5.6f, 0, .55f, 0, 32}},
        {{4.8, 1.2, -5.7, 0, 0.55, 0, 32}, {4.1, 1, -5, 0, 0.55, 0, 32}},
        {{2.6, 0.65, -2.9, 0.72, 0.4, -1.25, 30}, {2.1, 0.55, -2.4, 0.72, 0.4, -1.25, 30}},
        {{5.8, 1.25, 1, 0, 0.6, 0, 36}, {5.5, 1.05, -0.8, 0, 0.6, 0, 36}},
        {{-4.8, 1.5, 5.8, 0, 0.6, 0.3, 32}, {-4.1, 1.2, 5, 0, 0.6, 0.3, 32}},
        {{0, 1, -5, 0, 0.55, -0.5, 32}, {0.5, 0.8, -4.6, 0, 0.55, -0.5, 32}},
        {{0, 1.3, 5.6, 0, 0.7, 1, 32}, {-0.6, 1.1, 5.2, 0, 0.7, 1, 32}},
        {{-2.9, 4.8, -3.6, 0, 0.4, 0, 36}, {-1.5, 4.5, -3.8, 0, 0.4, 0, 36}},
        {{2.5, 4.5, 3.5, 0, 0.6, 0.4, 36}, {1, 4.3, 3.8, 0, 0.6, 0.4, 36}},
        {{-1.9, 0.8, -3.4, -0.65, 0.65, -1.75, 30}, {-1.5, 0.7, -3, -0.65, 0.65, -1.75, 30}},
        {{1.8, 0.9, 3.5, 0.6, 0.7, 1.65, 30}, {1.4, 0.8, 3.1, 0.6, 0.7, 1.65, 30}},
        {{-3, 0.75, 2.7, -0.7, 0.45, 1.2, 30}, {-2.5, 0.6, 2.3, -0.7, 0.45, 1.2, 30}},
        {{2.6, 2.5, -2.8, 0, 1, 0, 36}, {1.8, 2.2, -2.9, 0, 1, 0, 36}}};
    float time = fixed ? 0 : fmodf(fmaxf(0, seconds), LAUNCHER_SHOT_SECONDS * LAUNCHER_SHOTS);
    int shot = (int)(time / LAUNCHER_SHOT_SECONDS);
    float t = (time - shot * LAUNCHER_SHOT_SECONDS) / LAUNCHER_SHOT_SECONDS;
    t = t * t * (3 - 2 * t);
    for (int i = 0; i < 6; i++)
        look[i] = shots[shot][0][i] + (shots[shot][1][i] - shots[shot][0][i]) * t;
    look[6] = 0;
    look[7] = 1;
    look[8] = 0;
    *fov = shots[shot][0][6] + (shots[shot][1][6] - shots[shot][0][6]) * t;
}
int launcher_scene_saved_vehicle(const char *saves, struct gt4_vol *vol, char code[96], int *color,
                                 char wheel[32], char wing[32])
{
    wheel[0] = wing[0] = 0;
    strcpy(code, "ford_gt_05");
    *color = 0;
    gt4_save_init(saves);
    ah_vm *vm = ah_vm_new();
    ah_obj *game = ah_module_new(vm, "game", ah_root(vm), NULL, NULL);
    gt4_save_restore(vm, game);
    ah_val garage, cars, index, label, paint;
    ah_obj *car = NULL;
    if (ah_get_member(vm, game, "garage", &garage) &&
        ah_get_member(vm, ah_as_obj(garage, AO_MODULE), "cars", &cars) &&
        ah_get_member(vm, ah_as_obj(garage, AO_MODULE), "riding_index", &index))
        car = ah_as_obj(ah_array_get(ah_as_obj(cars, AO_ARRAY), (int)ah_num(index)), AO_MODULE);
    int found = car && ah_get_member(vm, car, "car_code", &label) && ah_cstr(label) &&
                *ah_cstr(label) && strlen(ah_cstr(label)) < 96;
    if (found) {
        strcpy(code, ah_cstr(label));
        if (ah_get_member(vm, car, "color_index", &paint))
            *color = (int)ah_num(paint);
    }
    if (found && vol) {
        uint8_t body[0x4b8];
        /* Original: func_00440938 (serialized body +0x468); fitted models use 00454270/00454378.
           Reuse the career decoder so the active setup sheet selects the wing. */
        if (gt4_game_car_body(vm, car, body))
            gt4_body_part_labels(vol, body, wheel, wing);
    }
    ah_vm_free(vm);
    gt4_save_shutdown();
    return found;
}
int launcher_scene_saved_car(const char *saves, char code[96], int *color)
{
    char wheel[32], wing[32];
    return launcher_scene_saved_vehicle(saves, NULL, code, color, wheel, wing);
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "platform/platform.h"
#include "plat_sdl.h"
#include "showroom.h"
#include "vol.h"
#include "gpb.h"
#include "tex1.h"
#include "video_gl.h"
typedef struct {
    LONG lock, frames, stop, selection, fixed, paused, time_ms;
    LONG render_us, read_us, convert_us, seek_serial, read_pixels;
    DWORD parent;
    char caption[256];
    uint32_t pixels[LAUNCHER_WIDTH * LAUNCHER_HEIGHT];
} scene_shared;
struct launcher_scene {
    HANDLE mapping, process;
    scene_shared *shared;
    uint32_t *last;
    int copied;
};
void launcher_scene_close(launcher_scene *s)
{
    if (!s)
        return;
    if (s->shared)
        InterlockedExchange(&s->shared->stop, 1);
    if (s->process) {
        if (WaitForSingleObject(s->process, 5000) == WAIT_TIMEOUT)
            TerminateProcess(s->process, 1);
        CloseHandle(s->process);
    }
    if (s->shared)
        UnmapViewOfFile(s->shared);
    if (s->mapping)
        CloseHandle(s->mapping);
    free(s->last);
    free(s);
}
launcher_scene *launcher_scene_open(const char *disc, const char *saves, int backend, int fixed)
{
    launcher_scene *s = calloc(1, sizeof *s);
    wchar_t mapping[100], executable[1024], iso[1200], save_path[1200], command[4000];
    if (!s)
        return NULL;
    swprintf(mapping, 100, L"Local\\MoonLauncher-%lu-%llu", GetCurrentProcessId(),
             GetTickCount64());
    s->mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                    sizeof(scene_shared), mapping);
    if (!s->mapping)
        goto fail;
    s->shared = MapViewOfFile(s->mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(scene_shared));
    s->last = malloc(sizeof s->shared->pixels);
    if (!s->shared || !s->last)
        goto fail;
    s->shared->parent = GetCurrentProcessId();
    s->shared->fixed = fixed;
    DWORD executable_length = GetModuleFileNameW(NULL, executable, 1024);
    if (!executable_length || executable_length >= 1024 ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, disc, -1, iso, 1200) ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, saves, -1, save_path, 1200))
        goto fail;
    swprintf(command, 4000, L"\"%ls\" --launcher-scene \"%ls\" \"%ls\" %d \"%ls\"", executable, iso,
             mapping, backend, save_path);
    STARTUPINFOW startup = {sizeof startup};
    PROCESS_INFORMATION process = {0};
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    if (!CreateProcessW(executable, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                        &startup, &process))
        goto fail;
    CloseHandle(process.hThread);
    s->process = process.hProcess;
    return s;
fail:
    launcher_scene_close(s);
    return NULL;
}
int launcher_scene_frames(const launcher_scene *s)
{
    return s && s->shared ? InterlockedCompareExchange(&s->shared->frames, 0, 0) : 0;
}
int launcher_scene_copy(launcher_scene *s, uint32_t *pixels, int selection)
{
    if (!s || !launcher_scene_frames(s))
        return 0;
    InterlockedExchange(&s->shared->selection, selection);
    if (InterlockedCompareExchange(&s->shared->lock, 1, 0) == 0) {
        memcpy(s->last, s->shared->pixels, sizeof s->shared->pixels);
        s->copied = 1;
        InterlockedExchange(&s->shared->lock, 0);
    }
    if (!s->copied)
        return 0;
    memcpy(pixels, s->last, sizeof s->shared->pixels);
    return 1;
}
const char *launcher_scene_caption(const launcher_scene *s)
{
    return s && launcher_scene_frames(s) ? s->shared->caption : "";
}
void launcher_scene_report(const launcher_scene *s)
{
    if (s && s->shared)
        printf("LAUNCHER WORKER render=%.3f read=%.3f convert=%.3f ms\n",
               s->shared->render_us / 1000., s->shared->read_us / 1000.,
               s->shared->convert_us / 1000.);
    if (s && s->shared)
        printf("LAUNCHER TRANSFER pixels=%ld\n", s->shared->read_pixels);
}
void launcher_scene_pause(launcher_scene *s, int paused)
{
    if (s)
        InterlockedExchange(&s->shared->paused, paused != 0);
}
void launcher_scene_seek(launcher_scene *s, float seconds)
{
    if (s) {
        InterlockedExchange(&s->shared->time_ms, (LONG)(fmaxf(0, seconds) * 1000));
        InterlockedExchange(&s->shared->fixed, 1);
        InterlockedIncrement(&s->shared->seek_serial);
    }
}
static plat_texture *checker_texture(const char *disc)
{
    const char *error;
    gt4_vol *vol = gt4_vol_open(disc, &error);
    size_t package_size = 0, size = 0;
    uint8_t *package =
        vol ? gt4_vol_read(vol, "/projects/option/US/OptionRoot.gpb", &package_size) : NULL;
    uint8_t *bytes =
        package ? gpb_load(package, package_size, "image/option/opt_bg_4.png", &size) : NULL;
    tex1_image *images = NULL;
    int count = bytes ? tex1_decode(bytes, size, &images) : 0;
    plat_texture *texture =
        count == 1 ? plat_texture_create(images[0].width, images[0].height, images[0].rgba) : NULL;
    if (count > 0)
        tex1_free(images, count);
    free(bytes);
    free(package);
    gt4_vol_close(vol);
    return texture;
}
static plat_texture *capture_outgoing(plat_texture *previous)
{
    /* A scene capture is an opaque photograph. Model destination alpha
       must not change its contribution to the presentation dissolve. */
    GLfloat clear[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    return video_capture_frame(previous);
}
typedef struct {
    GLuint framebuffer, texture;
    uint8_t *pixels;
} launcher_readback;
static int readback_open(launcher_readback *out)
{
    out->pixels = malloc((size_t)LAUNCHER_WIDTH * LAUNCHER_HEIGHT * 4);
    if (!out->pixels)
        return 0;
    glGenTextures(1, &out->texture);
    glBindTexture(GL_TEXTURE_2D, out->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, LAUNCHER_WIDTH, LAUNCHER_HEIGHT, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &out->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out->texture, 0);
    int valid = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, video_draw_framebuffer());
    return valid;
}
static uint8_t *readback_pixels(launcher_readback *out)
{
    /* P: keep the 2x render and reconstruct on the GPU. Only the final
       launcher-sized image crosses into host memory; rows remain bottom-up. */
    video_flush2d();
    GLuint frame = video_draw_framebuffer();
    GLboolean clipped;
    glGetBooleanv(GL_SCISSOR_TEST, &clipped);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, frame);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, out->framebuffer);
    glBlitFramebuffer(0, 0, LAUNCHER_WIDTH * 2, LAUNCHER_HEIGHT * 2, 0, 0, LAUNCHER_WIDTH,
                      LAUNCHER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, LAUNCHER_WIDTH, LAUNCHER_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, out->pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, frame);
    if (clipped)
        glEnable(GL_SCISSOR_TEST);
    return out->pixels;
}
static void readback_close(launcher_readback *out)
{
    glDeleteFramebuffers(1, &out->framebuffer);
    glDeleteTextures(1, &out->texture);
    free(out->pixels);
}
static int render_scene(const char *disc, const char *saves, scene_shared *shared, int backend)
{
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, shared->parent);
    if (!parent)
        return 1;
    SDL_setenv("OPENGT4_WINDOW_HIDDEN", "1", 1);
    SDL_setenv("OPENGT4_GRAPHICS_API", backend ? "vulkan" : "opengl", 1);
    plat_video_desc video = {LAUNCHER_WIDTH * 2, LAUNCHER_HEIGHT * 2, 0, 0, 0, backend};
    if (plat_video_init(&video)) {
        CloseHandle(parent);
        return 1;
    }
    video_set_menu_pacing(0);
    video_set_aspect(1);
    plat_video_set_view(640, 360);
    launcher_readback readback = {0};
    int cpu_resolve = getenv("MOON_LAUNCHER_CPU_RESOLVE") != NULL;
    if (!cpu_resolve && !readback_open(&readback)) {
        readback_close(&readback);
        plat_video_shutdown();
        CloseHandle(parent);
        return 1;
    }
    showroom *cars = showroom_open(disc);
    plat_texture *checker = checker_texture(disc), *outgoing = NULL;
    int previous_shot = -1;
    int hard_cut = getenv("MOON_LAUNCHER_HARD_CUT") != NULL;
    char code[96];
    int color;
    char wheel[32], wing[32];
    const char *error;
    gt4_vol *vol = gt4_vol_open(disc, &error);
    launcher_scene_saved_vehicle(saves, vol, code, &color, wheel, wing);
    gt4_vol_close(vol);
    if (cars) {
        showroom_set_selection(cars, code, color);
        showroom_set_wheel(cars, wheel);
        showroom_set_wing(cars, wing);
    }
    unsigned frame = 0, fixed_frames = 0;
    LONG seek_seen = -1;
    int failed = !cars;
    double render_sum = 0, read_sum = 0, convert_sum = 0;
    double tick_us = 1e6 / SDL_GetPerformanceFrequency();
    float sequence_time = 0;
    Uint64 previous_frame = 0;
    float tint[3] = {1, 1, 1};
    while (cars && !shared->stop && WaitForSingleObject(parent, 0) == WAIT_TIMEOUT &&
           !plat_should_quit()) {
        Uint64 start = SDL_GetTicks64();
        if (shared->paused) {
            previous_frame = 0;
            SDL_Delay(50);
            continue;
        }
        if (shared->fixed) {
            if (seek_seen != shared->seek_serial) {
                seek_seen = shared->seek_serial;
                fixed_frames = 0;
            }
            if (fixed_frames >= 24) {
                SDL_Delay(5);
                continue;
            }
        }
        int ready = showroom_prepare(cars, code);
        if (ready < 0) {
            failed = 1;
            break;
        }
        if (!ready) {
            SDL_Delay(10);
            continue;
        }
        showroom_face face = {0};
        face.w = 640;
        face.h = 360;
        face.x = 0;
        face.studio_presentation = 1;
        face.pose_valid = 1;
        face.look_valid = 1;
        float dt = previous_frame ? fminf((start - previous_frame) / 1000.f, .1f) : 0;
        previous_frame = start;
        sequence_time += dt;
        float camera_time = shared->fixed ? shared->time_ms / 1000.f : sequence_time;
        int shot = (int)(fmodf(fmaxf(0, camera_time), LAUNCHER_SHOT_SECONDS * LAUNCHER_SHOTS) /
                         LAUNCHER_SHOT_SECONDS);
        if (frame && shot != previous_shot && !hard_cut)
            outgoing = capture_outgoing(outgoing);
        previous_shot = shot;
        launcher_scene_camera(camera_time, 0, face.look, &face.view_angle);
        float dx = face.look[3] - face.look[0], dz = face.look[5] - face.look[2];
        float offset = .22f * tanf(face.view_angle * .00872664626f);
        face.look[3] += dz * offset;
        face.look[5] -= dx * offset;
        face.lights.count = 2;
        face.lights.direction[0][0] = -.3f;
        face.lights.direction[0][1] = -.8f;
        face.lights.direction[0][2] = -.6f;
        face.lights.direction[1][0] = .7f;
        face.lights.direction[1][1] = -.5f;
        face.lights.direction[1][2] = .5f;
        int selection = InterlockedCompareExchange(&shared->selection, 0, 0);
        for (int c = 0; c < 3; c++) {
            static const float palette[8][3] = {{1, 1, 1},       {.98f, .98f, 1},   {.92f, .96f, 1},
                                                {1, .97f, .91f}, {.96f, .98f, 1},   {.95f, 1, .97f},
                                                {1, 1, 1},       {.98f, .98f, .98f}};
            float target = palette[selection >= 0 && selection < 8 ? selection : 0][c];
            tint[c] += (target - tint[c]) * .08f;
            face.lights.ambient[0][c] = .48f * tint[c];
            face.lights.diffuse[0][c] = .8f * tint[c];
            face.lights.specular[0][c] = .5f;
            face.lights.diffuse[1][c] = .4f;
            face.lights.specular[1][c] = .35f;
        }
        Uint64 render_start = SDL_GetPerformanceCounter();
        plat_video_begin_frame();
        const uint8_t background[4] = {(uint8_t)(246 * tint[0]), (uint8_t)(242 * tint[1]),
                                       (uint8_t)(233 * tint[2]), 255};
        plat_draw_quad(NULL, 0, 0, 640, 360, 0, 0, 1, 1, background);
        if (checker) {
            const uint8_t faint[4] = {246, 237, 222, 34};
            plat_draw_quad(checker, 0, 0, 640, 360, 0, 0, 1, .75f, faint);
        }
        showroom_draw_face(cars, code, frame ? (shared->fixed ? 1.f / 30 : dt) : 1, &face);
        float transition = launcher_scene_transition(camera_time);
        if (outgoing && transition < 1) {
            const uint8_t mix[4] = {255, 255, 255, (uint8_t)lroundf((1 - transition) * 255)};
            plat_draw_quad(outgoing, 0, 0, 640, 360, 0, 1, 1, 0, mix);
        }
        Uint64 read_start = SDL_GetPerformanceCounter();
        int width = LAUNCHER_WIDTH, height = LAUNCHER_HEIGHT;
        uint8_t *pixels =
            cpu_resolve ? plat_sdl_read_pixels(&width, &height) : readback_pixels(&readback);
        Uint64 convert_start = SDL_GetPerformanceCounter();
        if (pixels && width == LAUNCHER_WIDTH * (cpu_resolve ? 2 : 1) &&
            height == LAUNCHER_HEIGHT * (cpu_resolve ? 2 : 1) &&
            InterlockedCompareExchange(&shared->lock, 1, 0) == 0) {
            shared->read_pixels = width * height;
            snprintf(shared->caption, sizeof shared->caption, "%s", showroom_caption(cars));
            /* P: 2x supersampling, box reconstruction into the launcher's DIB. */
            for (int y = 0; y < LAUNCHER_HEIGHT; y++)
                for (int x = 0; x < LAUNCHER_WIDTH; x++) {
                    if (!cpu_resolve) {
                        const uint8_t *p =
                            pixels + ((size_t)(LAUNCHER_HEIGHT - 1 - y) * width + x) * 4;
                        shared->pixels[y * LAUNCHER_WIDTH + x] = (p[0] << 16) | (p[1] << 8) | p[2];
                        continue;
                    }
                    const uint8_t *a = pixels + ((size_t)y * 2 * width + x * 2) * 4;
                    const uint8_t *b = a + width * 4;
                    unsigned r = (a[0] + a[4] + b[0] + b[4] + 2) / 4;
                    unsigned g = (a[1] + a[5] + b[1] + b[5] + 2) / 4;
                    unsigned blue = (a[2] + a[6] + b[2] + b[6] + 2) / 4;
                    shared->pixels[y * LAUNCHER_WIDTH + x] = (r << 16) | (g << 8) | blue;
                }
            render_sum += (read_start - render_start) * tick_us;
            read_sum += (convert_start - read_start) * tick_us;
            convert_sum += (SDL_GetPerformanceCounter() - convert_start) * tick_us;
            shared->render_us = (LONG)(render_sum / (frame + 1));
            shared->read_us = (LONG)(read_sum / (frame + 1));
            shared->convert_us = (LONG)(convert_sum / (frame + 1));
            InterlockedIncrement(&shared->frames);
            InterlockedExchange(&shared->lock, 0);
        }
        if (cpu_resolve)
            free(pixels);
        plat_video_end_frame();
        frame++;
        fixed_frames++;
        Uint64 elapsed = SDL_GetTicks64() - start;
        if (elapsed < LAUNCHER_INTERVAL_MS)
            SDL_Delay((Uint32)(LAUNCHER_INTERVAL_MS - elapsed));
    }
    readback_close(&readback);
    plat_texture_destroy(outgoing);
    plat_texture_destroy(checker);
    showroom_close(cars);
    plat_video_shutdown();
    CloseHandle(parent);
    return failed;
}
int launcher_scene_entry(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "--launcher-scene"))
        return -1;
    if (argc != 6)
        return 2;
    wchar_t name[100];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[3], -1, name, 100))
        return 2;
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    scene_shared *shared =
        mapping ? MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(scene_shared)) : NULL;
    int result = shared ? render_scene(argv[2], argv[5], shared, atoi(argv[4]) != 0) : 1;
    if (shared)
        UnmapViewOfFile(shared);
    if (mapping)
        CloseHandle(mapping);
    return result;
}
#else
void launcher_scene_report(const launcher_scene *s)
{
    (void)s;
}
const char *launcher_scene_caption(const launcher_scene *s)
{
    (void)s;
    return "";
}
void launcher_scene_pause(launcher_scene *s, int paused)
{
    (void)s;
    (void)paused;
}
void launcher_scene_seek(launcher_scene *s, float seconds)
{
    (void)s;
    (void)seconds;
}
struct launcher_scene {
    int unused;
};
launcher_scene *launcher_scene_open(const char *disc, const char *saves, int backend, int fixed)
{
    (void)disc;
    (void)saves;
    (void)backend;
    (void)fixed;
    return NULL;
}
void launcher_scene_close(launcher_scene *s)
{
    (void)s;
}
int launcher_scene_frames(const launcher_scene *s)
{
    (void)s;
    return 0;
}
int launcher_scene_copy(launcher_scene *s, uint32_t *p, int n)
{
    (void)s;
    (void)p;
    (void)n;
    return 0;
}
int launcher_scene_entry(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1;
}
#endif
