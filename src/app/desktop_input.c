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

int touch_dialog_input(ui_project *ui, int enabled, ui_widget **items, int count,
                       const plat_key_event *ev, touch_dialog_state *s, int *result)
{
    *result = 0;
    if (!enabled || ev->pad || ev->key) ui_touch_mode(ui,0);
    if (enabled && ev->touch==1) ui_touch_mode(ui,1);
    if (!enabled || ev->touch == 4 || ev->pad || ev->key) {
        s->active = 0;
        s->selected = NULL;
        ui_touch_preview(ui, NULL);
        ui_touch_press(ui, NULL);
        ui_pointer(ui, 0, 0, 0);
        return -1;
    }
    if (!ev->touch)
        return -1;
    float x = ev->touch_x * 640.f, y = ev->touch_y * video_view_height();
    int hit = -1;
    for (int i = 0; i < count; i++)
        if (items[i] && items[i]->visible && !items[i]->insensitive &&
            ui_pointer_contains(ui, items[i], x, y))
            hit = i;
    if (ev->touch == 1 && !s->active) {
        s->active = 1;
        s->id = ev->touch_id;
        s->index = hit;
        ui_touch_press(ui, hit >= 0 && items[hit] == s->selected ? s->selected : NULL);
        s->x = x;
        s->y = y;
        s->dragged = 0;
    } else if (s->active && s->id == ev->touch_id) {
        float dx = x - s->x, dy = y - s->y;
        if (dx * dx + dy * dy > 64) {
            s->dragged = 1;
            ui_touch_press(ui, NULL);
        }
        if (ev->touch == 3) {
            ui_touch_press(ui, NULL);
            if (hit >= 0 && hit == s->index && !s->dragged) {
                if (s->selected == items[hit]) {
                    *result = 1;
                    s->selected = NULL;
                } else
                    s->selected = items[hit];
            } else
                s->selected = NULL;
            s->active = 0;
        }
    }
    ui_touch_preview(ui, s->selected);
    return s->id == ev->touch_id ? hit : -1;
}

int touch_back_input(ui_project *ui, int enabled, const void *context, int root,
                     const plat_key_event *ev, touch_back_state *s, int *result)
{
    *result = 0;
    if (!enabled || s->context != context || s->root != root || ev->pad || ev->key ||
        ev->touch == 4) {
        s->tap = (touch_dialog_state){0};
        s->context = context;
        s->root = root;
        if (!enabled || ev->pad || ev->key || ev->touch == 4)
            return 0;
    }
    if (!ev->touch)
        return 0;
    ui_widget *back = ui_touch_back_widget(ui);
    if (!back || !back->visible || back->insensitive)
        return 0;
    float x = ev->touch_x * 640.f, y = ev->touch_y * 480.f;
    if (!s->tap.active) {
        if (ev->touch != 1)
            return 0;
        if (!ui_pointer_contains(ui, back, x, y)) {
            s->tap.selected = NULL;
            return 0;
        }
    }
    int action = 0;
    /* Back is painted in 640x480, independent of the preceding HUD view. */
    plat_key_event contact = *ev;
    contact.touch_y = y / video_view_height();
    touch_dialog_input(ui, 1, &back, 1, &contact, &s->tap, &action);
    *result = action == 1;
    return 1;
}
