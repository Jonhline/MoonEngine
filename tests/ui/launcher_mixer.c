#include "launcher_mixer.h"
#undef main
#include "../test_process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "mixer line %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static void key(launcher_mixer *m, SDL_Keycode code, int shift)
{
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = code;
    e.key.keysym.mod = shift ? KMOD_SHIFT : 0;
    launcher_mixer_event(m, &e, 0, 0);
}
static void mouse(launcher_mixer *m, Uint32 type, float x, float y)
{
    SDL_Event e = {0};
    e.type = type;
    e.button.button = SDL_BUTTON_LEFT;
    launcher_mixer_event(m, &e, x, y);
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    char path[1024];
    snprintf(path, sizeof path, "%s-%lu.ini", argv[1], test_process_id());
    moon_options store;
    CHECK(moon_options_load(&store,path,NULL));
    CHECK(moon_options_set(&store,"graphics.width","1920"));
    launcher_mixer m;
    launcher_mixer_load(&m, &store);
    CHECK(m.music==78 && m.effects==128 && !m.muted && m.dragging==-1);
    key(&m, SDLK_m, 0);
    if (argc > 2)
        m.effects = 0;
    CHECK(launcher_mixer_music(&m)==0 && m.effects==128 && m.music==78);
    SDL_Event repeat = {0};
    repeat.type = SDL_KEYDOWN;
    repeat.key.keysym.sym = SDLK_m;
    repeat.key.repeat = 1;
    CHECK(launcher_mixer_event(&m,&repeat,0,0) && m.muted);
    key(&m, SDLK_m, 0);
    CHECK(launcher_mixer_music(&m)==78);
    key(&m, SDLK_F8, 0);
    CHECK(m.open);
    key(&m, SDLK_DOWN, 0);
    key(&m, SDLK_LEFT, 0);
    key(&m, SDLK_RIGHT, 1);
    CHECK(m.row==1 && m.effects==121);
    key(&m, SDLK_ESCAPE, 0);
    CHECK(!m.open);
    mouse(&m, SDL_MOUSEBUTTONDOWN, 100, 75);
    CHECK(m.muted);
    mouse(&m, SDL_MOUSEBUTTONDOWN, 30, 75);
    CHECK(m.open);
    mouse(&m, SDL_MOUSEBUTTONDOWN, 142, 104);
    CHECK(m.music==64 && m.dragging==0);
    mouse(&m, SDL_MOUSEMOTION, 999, 104);
    CHECK(m.music==128);
    mouse(&m, SDL_MOUSEMOTION, 0, 104);
    CHECK(m.music==0);
    mouse(&m, SDL_MOUSEBUTTONUP, 999, 999);
    CHECK(m.dragging==-1);
    mouse(&m, SDL_MOUSEBUTTONDOWN, 142, 132);
    CHECK(m.effects==64 && m.music==0);
    SDL_Event wheel = {0};
    wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.y = -1;
    CHECK(launcher_mixer_event(&m, &wheel, 140, 132) && m.effects == 56);
    wheel.wheel.y = 1;
    CHECK(launcher_mixer_event(&m, &wheel, 140, 132) && m.effects == 64);
    SDL_Event lost = {0};
    lost.type = SDL_WINDOWEVENT;
    lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    launcher_mixer_event(&m, &lost, 0, 0);
    CHECK(m.dragging==-1);
    CHECK(launcher_mixer_save(&m,&store) && !m.dirty);
    moon_options restored;
    CHECK(moon_options_load(&restored,path,NULL));
    launcher_mixer after;
    launcher_mixer_load(&after, &restored);
    CHECK(after.music==0 && after.effects==64 && after.muted);
    CHECK(!strcmp(moon_options_get(&restored,"graphics.width",""),"1920"));
    key(&after, SDLK_m, 0);
    CHECK(!after.muted && after.effects==64);
    moon_options_set(&restored, "audio.launcher_music_volume", "999");
    launcher_mixer_load(&after, &restored);
    CHECK(after.music==78);
    remove(path);
    puts(
        "PASS launcher mixer: independent mute, sliders, shortcuts, drag cancellation, shared persistence");
    return 0;
}
