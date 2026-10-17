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
    CHECK(SDL_Init(SDL_INIT_EVENTS)==0);
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
    e.button.button=SDL_BUTTON_RIGHT;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.mouse_button==2);
    SDL_zero(e); e.type=SDL_MOUSEWHEEL; e.wheel.y=2; e.wheel.direction=SDL_MOUSEWHEEL_FLIPPED;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.wheel==-2 && key.mouse_absolute);
    SDL_zero(e); e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_w;
    CHECK(SDL_PushEvent(&e)==1 && next_event(&key) && key.key==SDLK_w && !key.mouse);
    SDL_Quit();
    puts("Desktop SDL events: absolute motion, left/right release, wheel and keyboard passed.");
    return 0;
}
