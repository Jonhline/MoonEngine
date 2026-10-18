#define SDL_MAIN_HANDLED
#include "plat_sdl.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"desktop events line %d: %s\n",__LINE__,#x); SDL_Quit(); return 1;} } while(0)
/* SDL's polling sentinel can end the current pump cycle before an event
   injected between two cycles. The app naturally polls again next frame. */
static int next_event(plat_key_event *key)
{
    for (int i=0;i<4;i++) if(plat_sdl_poll(key)) return 1;
    return 0;
}
int main(int argc,char **argv)
{
    (void)argv;
    CHECK(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO)==0);
    SDL_Window *window = SDL_CreateWindow("Shortcut test", 0, 0, 320, 240, SDL_WINDOW_HIDDEN);
    CHECK(window);
    unsigned full=SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_INPUT_FOCUS;
    CHECK(plat_sdl_mouse_should_confine(1,full));
    CHECK(!plat_sdl_mouse_should_confine(0,full));
    CHECK(!plat_sdl_mouse_should_confine(1,SDL_WINDOW_INPUT_FOCUS));
    CHECK(!plat_sdl_mouse_should_confine(1,full & ~SDL_WINDOW_INPUT_FOCUS));
    CHECK(!plat_sdl_mouse_should_confine(1,full | SDL_WINDOW_MINIMIZED));
    SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    SDL_Event e; SDL_zero(e);
    e.type=SDL_MOUSEMOTION; e.motion.x=123; e.motion.y=234;
    CHECK(SDL_PushEvent(&e)==1);
    plat_key_event key;
    CHECK(next_event(&key) && key.mouse && key.mouse_absolute && key.mouse_x==123 && key.mouse_y==234 && !key.pad && !key.key);
    SDL_zero(e); e.type=SDL_MOUSEBUTTONUP; e.button.button=SDL_BUTTON_LEFT;
    e.button.x=100; e.button.y=200;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key));
    if(argc>1) key.mouse_button=2;
    CHECK(key.mouse_absolute && key.mouse_button==1 && key.mouse_x==100 && key.mouse_y==200);
    e.type=SDL_MOUSEBUTTONDOWN;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.mouse_button==3);
    e.type=SDL_MOUSEMOTION;e.motion.state=SDL_BUTTON_LMASK;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.mouse_button==4);
    SDL_zero(e);e.type=SDL_MOUSEBUTTONUP;
    e.button.button=SDL_BUTTON_RIGHT;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.mouse_button==2);
    SDL_zero(e); e.type=SDL_MOUSEWHEEL; e.wheel.y=2; e.wheel.direction=SDL_MOUSEWHEEL_FLIPPED;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.wheel==-2 && key.mouse_absolute);
    SDL_zero(e); e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_w;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.key==SDLK_w && !key.mouse);
    SDL_zero(e);
    e.type = SDL_FINGERDOWN;
    e.tfinger.fingerId = 42;
    e.tfinger.x = .25f;
    e.tfinger.y = .75f;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key));
    CHECK(key.touch==1 && key.touch_id==42 && key.touch_x==.25f && key.touch_y==.75f && !key.mouse);
    e.type = SDL_FINGERMOTION;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.touch==2);
    e.type = SDL_FINGERUP;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.touch==3);
    SDL_zero(e);
    e.type = SDL_MOUSEBUTTONUP;
    e.button.which = SDL_TOUCH_MOUSEID;
    CHECK(SDL_PushEvent(&e)==1);
    SDL_zero(e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_ESCAPE;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && !key.mouse && key.key==SDLK_ESCAPE);
    SDL_zero(e);
    e.type = SDL_WINDOWEVENT;
    e.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.touch==4);
    for (int i = 0; i < 4; i++) {
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        SDL_zero(e);
        e.type = SDL_KEYDOWN;
        e.key.windowID = SDL_GetWindowID(window);
        e.key.keysym.sym = i < 2 ? SDLK_RETURN : SDLK_KP_ENTER;
        e.key.keysym.mod = i < 2 ? KMOD_LALT : KMOD_RALT;
        CHECK(SDL_PushEvent(&e) == 1);
        e.type = SDL_KEYUP;
        e.key.keysym.mod = 0;
        CHECK(SDL_PushEvent(&e) == 1);
        e.type = SDL_KEYDOWN;
        e.key.keysym.sym = SDLK_w;
        e.key.keysym.mod = 0;
        CHECK(SDL_PushEvent(&e) == 1);
        CHECK(next_event(&key) && key.key == SDLK_w);
        CHECK(((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0) == !(i & 1));
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("Desktop SDL events: absolute motion, left/right release, wheel and keyboard passed.");
    return 0;
}
