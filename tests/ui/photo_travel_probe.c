/* Native Photo Travel integration; no race runner or solver is opened.
   This verifies the PC rendering/storage boundary, not GS pixel parity. */
#include <SDL.h>
#include "platform/platform.h"
#include "plat_sdl.h"
#include "photo_travel.h"
#include "save.h"
#include "miniz.h"
#include "render3d.h"
#include "video_gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "photo probe line %d: %s\n", __LINE__, #x);                            \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static int root(menu *m, const char *name)
{
    for (int i = 0; i < ui_root_count(menu_ui(m)); i++)
        if (!strcmp(ui_root_name(menu_ui(m), i), name))
            return i;
    return -1;
}
static int warning_frames;
static int acknowledge_warning(menu *m,void *unused)
{
    (void)unused;CHECK(++warning_frames<120);
    menu_animate(m,1.f/60);
    menu_send_input(m,MENU_ACCEPT);
    menu_send_key(m,65293,1);
    return 1;
}
static void frames(menu *m, int n)
{
    for (int i = 0; i < n; i++) {
        menu_tick(m);
        menu_animate(m, 1.f / 60);
        plat_video_begin_frame();
        int d, s;
        ui_draw_root(menu_ui(m), menu_active_root(m), &d, &s);
        plat_video_end_frame();
    }
}
static void present(menu *m, float seconds, void *unused)
{
    (void)unused;
    int count = (int)ceilf(seconds * 60) + 1;
    for (int i = 0; i < count; i++) {
        menu_animate(m, 1.f / 60);
        plat_video_begin_frame();
        ui_draw_root(menu_ui(m), menu_active_root(m), NULL, NULL);
        plat_video_end_frame();
    }
}
static void dump(menu *m, const char *path)
{
    plat_video_begin_frame();
    int d, s, w, h;
    ui_draw_root(menu_ui(m), menu_active_root(m), &d, &s);
    uint8_t *rgba = plat_sdl_read_pixels(&w, &h);
    CHECK(rgba);
    size_t n;
    void *png = tdefl_write_image_to_png_file_in_memory(rgba, w, h, 4, &n);
    CHECK(png);
    FILE *f = fopen(path, "wb");
    CHECK(f && fwrite(png,1,n,f)==n);
    fclose(f);
    mz_free(png);
    free(rgba);
    plat_video_end_frame();
}
static uint64_t file_hash(const char *path, int w, int h)
{
    FILE *f = fopen(path, "rb");
    CHECK(f);
    uint8_t header[24];
    CHECK(fread(header,1,24,f)==24);
    CHECK(!memcmp(header,"\x89PNG\r\n\x1a\n",8));
    unsigned width = (unsigned)header[16] << 24 | (unsigned)header[17] << 16 |
                     (unsigned)header[18] << 8 | header[19];
    unsigned height = (unsigned)header[20] << 24 | (unsigned)header[21] << 16 |
                      (unsigned)header[22] << 8 | header[23];
    CHECK(width==(unsigned)w && height==(unsigned)h);
    rewind(f);
    uint64_t hash = 14695981039346656037ull;
    int c;
    while ((c = fgetc(f)) != EOF) {
        hash ^= (uint8_t)c;
        hash *= 1099511628211ull;
    }
    fclose(f);
    return hash;
}
int main(int argc, char **argv)
{
    CHECK(argc==4 || argc==5);
    int negative = argc == 5;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
    SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED, "0");
    CHECK(!plat_video_init(&(plat_video_desc){960,720,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    plat_video_set_view(640, 480);
    gt4_save_init(argv[2]);
    const char *error;
    gt4_vol *vol = gt4_vol_open(argv[1], &error);
    CHECK(vol);
    char msg[256];
    menu *m = menu_open(vol, "photo_shoot", msg, sizeof msg);
    CHECK(m);
    photo_travel *p = photo_travel_open(argv[1]);
    CHECK(p);
    const char *course = getenv("MOON_PHOTO_COURSE");
    CHECK(photo_travel_load(p,m,course?course:"ss_brooklyn","ka_01",1,NULL,NULL));
    if (getenv("MOON_PHOTO_ENHANCED")) {
        r3d_daylight_options(1,-1,1);
        r3d_night_sky(1);
        r3d_modern_lighting(1);
        r3d_material_shading(1);
        video_hdr_options(1,0);
    }
    int shoot = root(m, "ShootRoot"), preview = root(m, "PreviewRoot");
    CHECK(shoot>=0 && preview>=0);
    menu_enter(m, shoot);
    frames(m, 60);
    CHECK(menu_active_root(m)==shoot);
    gt4_photo_mode *s = menu_photo_state(m);
    CHECK(s->ready && s->view_angle==70);
    ui_widget *panel = ui_widget_child(ui_root_widget(menu_ui(m), shoot), "Panel"),
              *focal = ui_widget_child(panel, "FocalLen");
    CHECK(focal);
    ui_widget *label = ui_widget_child(focal, "label");
    CHECK(label);
    fprintf(stderr, "focal: value=%g min=%g max=%g step=%g label='%s'\n", focal->photo_value,
            focal->photo_min, focal->photo_max, focal->photo_step, label->text ? label->text : "");
    /* Focus alone cannot grab a camera tool: the original script owns
       press/release. */
    {
        ui_widget *pan = ui_widget_child(panel, "Pan");
        CHECK(pan);
        menu_native_focus(m, pan);
        CHECK(!strcmp(s->operate_mode,"none"));
        menu_send_input(m, MENU_ACCEPT);
        CHECK(!strcmp(s->operate_mode,"pan"));
        menu_send_key(m, 65293, 1);
        CHECK(!strcmp(s->operate_mode,"none"));
        if (getenv("GT4_PHOTO_HOLD_NEGATIVE"))
            CHECK(!strcmp(s->operate_mode,"pan"));
    }
    menu_native_focus(m, focal);
    menu_send_input(m, MENU_ACCEPT);
    menu_send_input(m, MENU_RIGHT);
    menu_send_input(m, MENU_ACCEPT);
    frames(m, 2);
    CHECK(s->view_angle>70 && isfinite(s->view_angle));
    /* Mouse wheel on the closed focal control must keep its target and
       increase the authored floating value, including repeated detents. */
    frames(m, 1);
    float px=focal->pointer_rect[0]+focal->pointer_rect[2]*.5f;
    float py=focal->pointer_rect[1]+focal->pointer_rect[3]*.5f;
    float wheel_before=s->view_angle;
    for (int i=0;i<12;i++) {
        if (getenv("MOON_PHOTO_POINTER_NEGATIVE")) menu_send_input(m,MENU_RIGHT);
        else menu_pointer(m,1,px,py,0,1);
        frames(m,1);
        CHECK(s->view_angle>wheel_before);
        wheel_before=s->view_angle;
    }
    menu_send_input(m,MENU_ACCEPT);
    frames(m,2);
    CHECK(s->view_angle==wheel_before);
    /* The authored popup knob also accepts continuous direct dragging. */
    ui_widget *popup=ui_widget_child(focal,"popup");
    ui_widget *pinch=ui_widget_child(popup,"pinch");
    CHECK(popup && pinch);
    menu_pointer(m,1,px,py,3,0);
    float drag_x=focal->pointer_rect[0]+popup->x+popup->w-pinch->w*.5f;
    menu_pointer(m,1,drag_x,py,4,0);
    menu_pointer(m,1,drag_x,py,1,0);
    float dragged=s->view_angle;
    CHECK(dragged>290 && dragged<=300);
    menu_pointer(m,1,630,470,0,0);
    frames(m,3);
    CHECK(s->view_angle==dragged);
    /* Restore a useful framing after checking the far end of the zoom. */
    menu_pointer(m,1,px,py,3,0);
    drag_x=focal->pointer_rect[0]+popup->x+pinch->w*.5f+(popup->w-pinch->w)*(70.f-16)/(300-16);
    menu_pointer(m,1,drag_x,py,4,0);
    menu_pointer(m,1,drag_x,py,1,0);
    menu_send_input(m,MENU_ACCEPT);
    frames(m,1);
    /* Original can_shoot is writable and defaults true; no invented occlusion gate. */
    CHECK(s->can_shoot);
    menu_set_event_presenter(m,acknowledge_warning,NULL);
    s->can_shoot=0;
    if(getenv("MOON_PHOTO_SHOOT_NEGATIVE"))s->can_shoot=1;
    photo_travel_shoot(p);frames(m,60);CHECK(menu_active_root(m)==shoot);
    CHECK(warning_frames>0);
    menu_set_event_presenter(m,NULL,NULL);
    menu_set_presenter(m,present,NULL);
    s->can_shoot=1;
    photo_travel_shoot(p);
    frames(m, 60);
    CHECK(menu_active_root(m)==preview && s->image_width>0);
    dump(m, argv[3]);
    menu_send_input(m, MENU_ACCEPT);
    frames(m, 2);
    char first[1200];
    snprintf(first, sizeof first, "%s", photo_travel_last_saved(p));
    CHECK(*first);
    uint64_t hash = file_hash(first, s->image_width, s->image_height);
    s->car[0] += 10;
    s->yaw += 15; /* Frozen preview must not change when the live camera changes. */
    menu_send_input(m, MENU_ACCEPT);
    frames(m, 2);
    CHECK(strcmp(first,photo_travel_last_saved(p)) && hash==file_hash(photo_travel_last_saved(p),s->image_width,s->image_height));
    if (negative)
        hash ^= 1;
    CHECK(hash==file_hash(first,s->image_width,s->image_height));
    menu_send_input(m, MENU_BACK);
    frames(m, 60);
    CHECK(menu_active_root(m)==shoot && !s->image_width);
    fprintf(stderr,"return fade=%g remaining=%g\n",ui_fade_opacity(menu_ui(m)),ui_fade_remaining(menu_ui(m)));
    if (getenv("MOON_PHOTO_RETURN_NEGATIVE")) ui_fade(menu_ui(m),1,1,0);
    CHECK(ui_fade_opacity(menu_ui(m))==0);
    char returned[1200];snprintf(returned,sizeof returned,"%s-return.png",argv[3]);
    dump(m,returned);
    photo_travel_shoot(p);
    frames(m, 60);
    CHECK(menu_active_root(m)==preview);
    menu_send_input(m, MENU_ACCEPT);
    frames(m, 2);
    CHECK(hash!=file_hash(photo_travel_last_saved(p),s->image_width,s->image_height));
    menu_send_input(m, MENU_BACK);
    frames(m, 60);
    CHECK(photo_travel_load(p,m,"ss_gion","ka_01",1,NULL,NULL));
    menu_enter(m, shoot);
    frames(m, 60);
    CHECK(menu_active_root(m)==shoot && s->ready);
    char requested_course[32], car[96];
    int color;
    CHECK(menu_photo_request(m,requested_course,car,&color));
    CHECK(!strcmp(requested_course,"ss_gion") && !strcmp(car,"ka_01") && color==1);
    puts(
        "PASS authored shot/preview/return/re-entry, numeric focal control, frozen PNG, unique exports, scene change; negative corrupts export hash. Rendering remains A.");
    photo_travel_close(p);
    menu_close(m);
    gt4_vol_close(vol);
    gt4_save_shutdown();
    plat_video_shutdown();
    return 0;
}
