/*
 * The race's pause menu (race_pause.h): the movie and the callbacks of
 * func_003C3260, by name (the script calls them in mixed case).
 */
#include "race_pause.h"
#include "rtext.h"
#include "strobe_player.h"
#include "save.h"
#include "ui.h"
#include "platform/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct race_pause {
    strb_player *movie;
    gt4_vol *volume;
    ui_project *cursor;
    int pointer_active;
    struct {
        long long id;
        int active, dragged, confirm, place, selected_place;
        float x, y;
        const void *owner, *selected;
    } touch;
    float pointer_x,pointer_y;
    int state, retry, screen, confirm;
    int replay;
    int nav; /* a navigation asked while the movie opened: 1 vertical, 2 all */
    race_pause_sound sound;
    void *user;
    uint8_t *rt_data;
    gt4_rtext rt;
    int rt_ok;
};

/* Original: func_003C3238 +0x00..0x20 selects brightness or contrast.
   Retain standalone values when no profile store has been initialized. */
static int screen_value[2][4];

static const char *display_option(int replay)
{
    /* Original: func_003EFEA8 +0x10..0x18; func_003EFEE8 +0x10..0x18. */
    return replay ? "replay_color_correction" : "race_color_correction";
}

static int named(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return 0;
    return !*a && !*b;
}

static double arg_num(const strb_value *v)
{
    return v->t == STRB_STR ? atof(v->s) : v->n;
}

static int call(void *user, const char *name, const strb_value *args, int nargs, strb_value *ret)
{
    race_pause *p = user;
    memset(ret, 0, sizeof *ret);
    if (named(name, "PLAY_SOUND")) {
        if (p->sound && nargs > 0 && args[0].t == STRB_STR)
            p->sound(p->user, args[0].s);
    } else if (named(name, "SETVAL")) { /* (name, value) */
        if (nargs > 1) {
            /* Original: func_003C3260 +0xEC..0x220: preserve all four
               options and change only the selected brightness/contrast. */
            int options[4];
            race_pause_display_options_for(p->replay, options);
            options[named(args[0].s, "BRIGHTNESS") ? 0 : 1] = (int)arg_num(&args[1]);
            memcpy(screen_value[p->replay], options, sizeof options);
            gt4_save_option_array_set(display_option(p->replay), options, 4);
        }
    } else if (named(name, "DEFAULTVAL")) { /* (name): the value kept */
        ret->t = STRB_NUM;
        int options[4];
        race_pause_display_options_for(p->replay, options);
        ret->n = nargs > 0 ? options[named(args[0].s, "BRIGHTNESS") ? 0 : 1] : 0;
    } else if (named(name, "EXIT_MENU")) { /* "exit" 4, "retry" 5, "continue" 3 */
        const char *s = nargs > 0 && args[0].t == STRB_STR ? args[0].s : "";
        p->state = !strcmp(s, "exit")    ? RACE_PAUSE_EXIT
                   : !strcmp(s, "retry") ? RACE_PAUSE_RETRY
                                         : RACE_PAUSE_CONTINUE;
    } else if (named(name, "RETRYENABLE") || named(name, "SCREENSETTINGENABLE") ||
               named(name, "CONFIRMEXIT")) {
        ret->t = STRB_BOOL;
        ret->n = named(name, "RETRYENABLE")   ? p->retry
                 : named(name, "CONFIRMEXIT") ? p->confirm
                                              : p->screen;
    } else if (named(name, "NAVIGATEVERTICAL") || named(name, "NAVIGATEALL")) {
        /* the movie's first frames run inside strb_open, before p->movie */
        if (p->movie)
            strb_navigate(p->movie, named(name, "NAVIGATEALL"));
        else
            p->nav = named(name, "NAVIGATEALL") ? 2 : 1;
    } else
        return 0;
    return 1;
}

race_pause *race_pause_open(gt4_vol *vol, int retry, int screen, int confirm,
                            race_pause_sound sound, void *user)
{
    return race_pause_open_display(vol, retry, screen, confirm, sound, user, 0);
}
race_pause *race_pause_open_display(gt4_vol *vol, int retry, int screen, int confirm,
                                    race_pause_sound sound, void *user, int replay)
{
    race_pause *p = calloc(1, sizeof *p);
    strb_host host;
    if (!p)
        return NULL;
    p->volume = vol;
    p->retry = retry;
    p->replay = replay != 0;
    p->screen = screen;
    p->confirm = confirm;
    p->sound = sound;
    p->user = user;
    memset(&host, 0, sizeof host);
    host.call = call;
    host.user = p;
    /* its fonts have no "#": the race's texts leave the ink alone (the
       PCSX2 pause: black labels without an outline, as the replay's
       hn_14 message) */
    host.palette = 1;
    /* the movie's texts are keys of the race's rtext, page RaceDisplay
       as the results' ("Screen Setting" -> "Screen Settings", the PCSX2
       pause) */
    {
        size_t n = 0;
        p->rt_data = gt4_vol_read(vol, "/rtext/raceUS.rt2", &n);
        p->rt_ok = p->rt_data && gt4_rtext_open(&p->rt, p->rt_data, n);
    }
    p->movie =
        strb_open(vol, "/menu/common/pause.strb", p->rt_ok ? &p->rt : NULL, "RaceDisplay", &host);
    if (!p->movie) {
        free(p->rt_data);
        free(p);
        return NULL;
    }
    /* 00474F38 +0x4C..0x60: input navigator starts with +0x14/+0x18
       both 1 (NavigateAll). pause.strb's menu_no_screen label skips
       frames 3/19 which request it explicitly; Battle2P uses that label. */
    strb_navigate(p->movie, p->nav != 1);
    return p;
}

void race_pause_close(race_pause *p)
{
    if (!p)
        return;
    strb_close(p->movie);
    ui_project_free(p->cursor);
    free(p->rt_data);
    free(p);
}

void race_pause_key(race_pause *p, int code)
{
    if (p)
        memset(&p->touch, 0, sizeof p->touch);
    if (p) {
        p->pointer_active=0;
        strb_key(p->movie, code);
    }
}
void race_pause_update(race_pause *p, double dt)
{
    if (p)
        strb_tick(p->movie, dt);
}
int race_pause_pointer(race_pause *p,int enabled,float x,float y,int action)
{
    if(!p) return 0;
    p->pointer_active=enabled;
    if(!enabled) return 0;
    p->pointer_x=x;p->pointer_y=y;
    if(!p->cursor) {char error[128];p->cursor=ui_project_load(p->volume,"cursor",error,sizeof error);}
    if(action==2) strb_key(p->movie,27);
    return strb_pointer(p->movie,x,y,action==1);
}
int race_pause_touch(race_pause *p, int phase, long long id, float x, float y)
{
    if (!p)
        return 0;
    p->pointer_active = 0; /* Touch uses the authored selected actor, not a mouse arrow. */
    if (phase == 4) {
        memset(&p->touch, 0, sizeof p->touch);
        return 0;
    }
    if (phase == 1 && !p->touch.active) {
        if (!strb_pointer(p->movie, x, y, 0)) {
            p->touch.selected = NULL;
            return 0;
        }
        strb_pointer_selection(p->movie, &p->touch.owner, &p->touch.place);
        p->touch.confirm =
            p->touch.owner == p->touch.selected && p->touch.place == p->touch.selected_place;
        p->touch.active = 1;
        p->touch.dragged = 0;
        p->touch.id = id;
        p->touch.x = x;
        p->touch.y = y;
        return 1;
    }
    if (!p->touch.active || p->touch.id != id)
        return 0;
    float dx = x - p->touch.x, dy = y - p->touch.y;
    if (dx * dx + dy * dy > 144)
        p->touch.dragged = 1;
    if (phase == 3) {
        const void *owner = NULL;
        int place = -1;
        int hit = strb_pointer(p->movie, x, y, 0);
        strb_pointer_selection(p->movie, &owner, &place);
        int valid = hit && !p->touch.dragged && owner == p->touch.owner && place == p->touch.place;
        p->touch.selected = valid ? owner : NULL;
        p->touch.selected_place = place;
        p->touch.active = 0;
        if (valid && p->touch.confirm) {
            p->touch.selected = NULL;
            strb_pointer(p->movie, x, y, 1);
        }
    }
    return 1;
}
void race_pause_draw(race_pause *p)
{
    if (p) {
        strb_draw(p->movie);
        if(p->pointer_active && p->cursor) {
            plat_video_set_view(640,480);
            ui_image_draw(p->cursor,"image/cursor/cursor.png",p->pointer_x-16,p->pointer_y,32,32,1);
            plat_video_set_view(640,448);
        }
    }
}
int race_pause_state(const race_pause *p)
{
    return p ? p->state : RACE_PAUSE_CONTINUE;
}

void race_pause_display_options(int options[4])
{
    race_pause_display_options_for(0, options);
}
void race_pause_display_options_for(int replay, int options[4])
{
    /* Original: func_0033F2E8 +0x458..0x470: race versus replay options. */
    replay = replay != 0;
    if (!gt4_save_option_array_get(display_option(replay), options, 4))
        memcpy(options, screen_value[replay], sizeof screen_value[replay]);
}
