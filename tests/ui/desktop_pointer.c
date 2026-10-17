#define plat_draw_rotated fixture_draw_rotated
#define main setting_fixture_main
#include "../gtmode/setting_menu.c"
#undef main
#undef plat_draw_rotated
#include "desktop_input.h"
SDL_GameController *plat_sdl_pad(int index) { (void)index; return NULL; }
static float cursor_tip[2], cursor_angle, cursor_size;
static int cursor_draws, cursor_alpha;
void plat_draw_rotated(const plat_texture *t,float x,float y,float w,float h,float cx,float cy,float a,const uint8_t c[4])
{ (void)t;(void)x;(void)y;(void)w;(void)h;cursor_angle=a;cursor_size=w;cursor_draws++;cursor_alpha=c[3]; cursor_tip[0]=cx;cursor_tip[1]=cy; }
int video_window_to_ui(int x,int y,float *u,float *v)
{ *u=(float)x; *v=(float)y; return x>=0 && x<640 && y>=0 && y<480; }
static void paint(menu *m)
{
    menu_animate(m, 1.f);
    ui_draw_root(menu_ui(m), menu_active_root(m), NULL, NULL);
}
static unsigned painted_faces(ui_project *ui,const ui_widget *w)
{
    unsigned h=w->drawn_tick==ui_draw_tick(ui)?(unsigned)(uintptr_t)w:0;
    for(int i=0;i<w->nkids;i++) h=h*16777619u^painted_faces(ui,w->kids[i]);
    return h;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    const char *err = NULL;
    char msg[256];
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    CHECK(vol);
    menu *m = menu_open(vol, "option", msg, sizeof msg);
    CHECK(m && page(m, "UnitRoot") >= 0);
    ticks(m);
    paint(m);
    ui_widget *pane = child(ui_root_widget(m->ui, menu_active_root(m)), "Pane");
    ui_widget *power = child(child(pane,"Power"),"OptionMenu");
    ui_widget *torque = child(child(pane,"Torque"),"OptionMenu");
    CHECK(power && torque);
    float x = power->pointer_rect[0] + power->pointer_rect[2] / 2;
    float y = power->pointer_rect[1] + power->pointer_rect[3] / 2;
    CHECK(ui_pointer_contains(m->ui,power,x,y));
    ui_widget *old = m->focus[menu_active_root(m)];
    int enabled = argc > 2; /* Negative fixture intentionally breaks Classic. */
    menu_pointer(m,enabled,x,y,1,0);
    CHECK(m->focus[menu_active_root(m)] == old && !power->option_open);
    plat_key_event key = {0};
    key.key = SDLK_w;
    CHECK(!desktop_input(m,0,&key) && key.key == SDLK_w);
    CHECK(!desktop_input(m,1,&key) && key.key == SDLK_UP);
    key.key=SDLK_s; key.pad=1;
    desktop_input(m,1,&key);
    CHECK(key.key==SDLK_s);
    key.pad=0; strcpy(key.text,"a"); key.key=SDLK_a;
    desktop_input(m,1,&key);
    CHECK(key.key==SDLK_a && !strcmp(key.text,"a"));
    CHECK(menu_pointer(m,1,x,y,0,0));
    CHECK(m->focus[menu_active_root(m)] == old);
    paint(m);
    CHECK(cursor_tip[0] == x && cursor_tip[1] == y);
    CHECK(menu_pointer(m,1,x,y,1,0) && power->option_open);
    paint(m);
    ui_widget *item = ui_option_item(power,2);
    CHECK(item && item->pointer_rect[2] > 0);
    x = item->pointer_rect[0] + item->pointer_rect[2]/2;
    y = item->pointer_rect[1] + item->pointer_rect[3]/2;
    unsigned before_popup_hover=painted_faces(m->ui,item);
    CHECK(menu_pointer(m,1,x,y,0,0) && power->option_pending != 2);
    paint(m);CHECK(painted_faces(m->ui,item)!=before_popup_hover);
    CHECK(menu_pointer(m,1,x,y,1,0) && !power->option_open && power->focus_index == 2);
    paint(m);
    x = torque->pointer_rect[0] + torque->pointer_rect[2]/2;
    y = torque->pointer_rect[1] + torque->pointer_rect[3]/2;
    CHECK(menu_pointer(m,1,x,y,0,0) && m->focus[menu_active_root(m)] == power);
    torque->insensitive = 1;
    menu_pointer(m,1,x,y,1,0);
    CHECK(!torque->option_open);
    torque->insensitive = 0;
    menu_send_input(m,MENU_UP);
    CHECK(m->focus[menu_active_root(m)] != torque);
    /* Hidden controls and previous-page geometry must not be clickable. */
    power->visible=0;
    paint(m);
    CHECK(!ui_pointer_contains(m->ui,power,power->pointer_rect[0]+1,power->pointer_rect[1]+1));
    CHECK(!menu_pointer(m,1,-10,-10,1,0));
    /* The native confirmation/profile picker uses the same painted hit API. */
    paint(m);
    ui_widget *dialog_items[] = {torque};
    plat_key_event click = {0};
    click.mouse=click.mouse_absolute=1;
    click.mouse_button=1;
    click.mouse_x=(int)(torque->pointer_rect[0]+torque->pointer_rect[2]/2);
    click.mouse_y=(int)(torque->pointer_rect[1]+torque->pointer_rect[3]/2);
    int action=-1;
    CHECK(desktop_dialog_input(m->ui,0,dialog_items,1,&click,&action)==-1 && !action);
    CHECK(desktop_dialog_input(m->ui,1,dialog_items,1,&click,&action)==0 && action==1);
    click.mouse_button=2; click.mouse_x=5; click.mouse_y=5;
    CHECK(desktop_dialog_input(m->ui,1,dialog_items,1,&click,&action)==-1 && action==2);
    /* Real cursor project's native confirmation: hover lights OK without
       controller focus, and opening/animation never relocates the mouse. */
    ui_project *dialog=ui_project_load(vol,"cursor",msg,sizeof msg);CHECK(dialog);
    int dialog_root=-1;
    for(int i=0;i<ui_root_count(dialog);i++) if(!strcmp(ui_root_name(dialog,i),"ConfirmDialog")) dialog_root=i;
    CHECK(dialog_root>=0);
    ui_widget *dialog_page=ui_root_widget(dialog,dialog_root), *dialog_pane=child(dialog_page,"Dialog");
    dialog_page->visible=1;dialog_page->opacity=1;dialog_pane->y=100;
    ui_widget *buttons=child(dialog_pane,"Buttons"), *pair=child(buttons,"YesNo");
    child(buttons,"OK")->visible=0;pair->visible=1;
    ui_widget *yes=child(pair,"Yes"),*no=child(pair,"No");
    yes->focused=0;no->focused=1;
    ui_cursor_select(dialog,no);ui_pointer(dialog,1,1,1);ui_pointer_hover(dialog,NULL);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    unsigned unlit=painted_faces(dialog,yes);
    click.mouse_button=0;click.mouse_x=(int)(yes->pointer_rect[0]+yes->pointer_rect[2]/2);
    click.mouse_y=(int)(yes->pointer_rect[1]+yes->pointer_rect[3]/2);
    ui_widget *native_items[]={no,yes};
    CHECK(desktop_dialog_input(dialog,1,native_items,2,&click,&action)==1 && !action && !yes->focused);
    ui_animate(dialog,dialog_root,1.f);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(painted_faces(dialog,yes)!=unlit);
    CHECK(cursor_tip[0]==click.mouse_x && cursor_tip[1]==click.mouse_y);
    /* Optional smoothing has no first-sample popup pull, brief monotonic
       settling, and the same position at 30/60 Hz. Raw mode stays exact. */
    ui_pointer_smoothing(dialog,1);
    ui_pointer(dialog,0,0,0);ui_pointer(dialog,1,100,100);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_tip[0]==100 && cursor_tip[1]==100);
    ui_pointer(dialog,1,120,110);ui_animate(dialog,dialog_root,1.f/30);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    float at30x=cursor_tip[0],at30y=cursor_tip[1];
    CHECK(at30x>100 && at30x<120 && at30y>100 && at30y<110);
    ui_pointer(dialog,0,0,0);ui_pointer(dialog,1,100,100);ui_pointer(dialog,1,120,110);
    ui_animate(dialog,dialog_root,1.f/60);ui_animate(dialog,dialog_root,1.f/60);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(fabsf(cursor_tip[0]-at30x)<.001f && fabsf(cursor_tip[1]-at30y)<.001f);
    ui_animate(dialog,dialog_root,.5f);ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_tip[0]==120 && cursor_tip[1]==110);
    float old_angle=cursor_angle;
    ui_widget *right=ui_widget_from_text(dialog,dialog_root,
        "Composite { name string {\"right\"} cursor_align digit {4} }");
    CHECK(right);ui_pointer_hover(dialog,right);
    ui_animate(dialog,dialog_root,.02f);ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_angle<old_angle && cursor_angle>-45);
    ui_animate(dialog,dialog_root,.6f);ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(fabsf(cursor_angle+45)<.01f);
    /* Clickable targets communicate continuously, without region flips. */
    ui_widget *context=ui_widget_from_text(dialog,dialog_root,
        "Composite { name string {\"context\"} }");
    CHECK(context);context->pointer_rect[0]=100;context->pointer_rect[1]=100;
    context->pointer_rect[2]=300;context->pointer_rect[3]=60;
    ui_pointer_hover(dialog,context);ui_pointer(dialog,1,110,130);
    ui_animate(dialog,dialog_root,.6f);ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_angle>-2 && cursor_angle<1 && cursor_size>33);
    ui_pointer(dialog,1,390,130);ui_animate(dialog,dialog_root,.02f);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(isfinite(cursor_angle) && cursor_angle>-60 && cursor_angle<30);
    ui_animate(dialog,dialog_root,.6f);ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_angle> -17 && cursor_angle< -14);
    ui_pointer(dialog,0,0,0);ui_pointer(dialog,1,200,130);
    ui_animate(dialog,dialog_root,.5f);ui_draw_root(dialog,dialog_root,NULL,NULL);
    float resting_angle=cursor_angle;
    ui_pointer(dialog,1,220,130);ui_animate(dialog,dialog_root,.02f);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_angle<resting_angle && cursor_angle>resting_angle-15);
    ui_pointer(dialog,1,180,130);
    for(int frame=0;frame<10;frame++) {
        ui_animate(dialog,dialog_root,1.f/60);ui_draw_root(dialog,dialog_root,NULL,NULL);
        CHECK(isfinite(cursor_angle) && fabsf(cursor_angle-resting_angle)<15);
    }
    ui_animate(dialog,dialog_root,.6f);ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(fabsf(cursor_angle+4.266667f)<.1f);
    ui_pointer(dialog,1,199,130);ui_animate(dialog,dialog_root,.6f);
    ui_draw_root(dialog,dialog_root,NULL,NULL);float middle_angle=cursor_angle;
    ui_pointer(dialog,1,201,130);ui_animate(dialog,dialog_root,.6f);
    ui_draw_root(dialog,dialog_root,NULL,NULL);CHECK(fabsf(cursor_angle-middle_angle)<.2f);
    ui_pointer_hover(dialog,NULL);ui_animate(dialog,dialog_root,.6f);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(fabsf(cursor_angle+22.5f)<.1f && cursor_size<32.01f);
    ui_widget *strip=ui_widget_from_text(dialog,dialog_root,
        "SelectBox { name string {\"MouseStrip\"} geometry rectangle {100 100 200 50} direction digit {0} children [2] { Composite { name string {\"A\"} geometry rectangle {0 0 160 50} } Composite { name string {\"B\"} geometry rectangle {0 0 160 50} } } }");
    CHECK(strip);ui_widget_append(dialog_page,strip);ui_draw_root(dialog,dialog_root,NULL,NULL);
    ui_pointer(dialog,1,295,125);ui_pointer_edge_scroll(dialog,dialog_root,.02f);
    ui_animate(dialog,dialog_root,.6f);ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(fabsf(cursor_angle-90)<.1f);
    ui_pointer(dialog,1,200,125);ui_pointer_edge_scroll(dialog,dialog_root,.02f);
    uint8_t pixel[]={255,255,255,255};
    ui_image_put(dialog,"image/cursor/desktop_test.png",1,1,pixel);
    ui_cursor_icon(dialog,"desktop_test");ui_animate(dialog,dialog_root,.045f);
    cursor_draws=0;ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_draws==2 && cursor_alpha>120 && cursor_alpha<135);
    ui_animate(dialog,dialog_root,.1f);cursor_draws=0;ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_draws==1 && cursor_alpha==255);
    ui_pointer_smoothing(dialog,0);ui_pointer(dialog,1,180,160);
    ui_draw_root(dialog,dialog_root,NULL,NULL);
    CHECK(cursor_tip[0]==180 && cursor_tip[1]==160);
    ui_project_free(dialog);
    CHECK(!errors);
    menu_close(m);
    gt4_vol_close(vol);
    puts("Desktop pointer: Classic no-op, hover, dropdown selection, disabled/hidden and pad handover passed.");
    return 0;
}
