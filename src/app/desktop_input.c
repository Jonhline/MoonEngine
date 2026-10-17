#include "desktop_input.h"
#include "video_gl.h"
void desktop_input_hints(menu *m, int enabled, const plat_key_event *ev)
{
#ifndef __SWITCH__
    if(!m) return;
    if(!enabled) {ui_input_hints(menu_ui(m),0);return;}
    if(ev->pad) {
        ui_input_hints(menu_ui(m),ev->pad_type);
    } else if(ev->mouse || ev->key || ev->text[0]) ui_input_hints(menu_ui(m),1);
#else
    (void)m; (void)enabled; (void)ev;
#endif
}
int desktop_input(menu *m, int enabled, plat_key_event *ev)
{
#ifndef __SWITCH__
    if (ev->mouse && ev->mouse_absolute) {
        float x = -1, y = -1;
        int inside = video_window_to_ui(ev->mouse_x, ev->mouse_y, &x, &y);
        menu_pointer(m, enabled && inside, x, y, ev->mouse_button, ev->wheel);
        return 1;
    }
    if (enabled && !ev->pad && !ev->text[0]) {
        if (ev->key == SDLK_w) ev->key = SDLK_UP;
        if (ev->key == SDLK_s) ev->key = SDLK_DOWN;
        if (ev->key == SDLK_a) ev->key = SDLK_LEFT;
        if (ev->key == SDLK_d) ev->key = SDLK_RIGHT;
    }
#else
    (void)m;
    (void)enabled;
    (void)ev;
#endif
    return 0;
}
int desktop_dialog_input(ui_project *ui, int enabled, ui_widget **items, int count,
                         plat_key_event *ev, int *result)
{
    *result = 0;
#ifndef __SWITCH__
    if (!ev->mouse) { ui_pointer(ui, 0, 0, 0); return -1; }
    if (!ev->mouse_absolute) return -1;
    float x=-1, y=-1;
    int inside = video_window_to_ui(ev->mouse_x,ev->mouse_y,&x,&y);
    ui_pointer(ui,enabled && inside,x,y);
    if (!enabled || !inside) return -1;
    ui_pointer_hover(ui,NULL);
    if (ev->mouse_button == 2) *result=2;
    for (int i=0;i<count;i++)
        if (items[i]->visible && !items[i]->insensitive && ui_pointer_contains(ui,items[i],x,y)) {
            ui_pointer_hover(ui,items[i]);
            if (ev->mouse_button == 1) *result=1;
            return i;
        }
#else
    (void)ui; (void)enabled; (void)items; (void)count; (void)ev;
#endif
    return -1;
}
