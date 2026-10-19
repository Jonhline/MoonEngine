/* Exercise the disc's actual Arcade CarRoot and inspect its text widgets. */
#ifdef MOON_SPECS_SCREENSHOT
#include <SDL.h>
#include "plat_sdl.h"
#include "miniz.h"
#include "../../src/core/menu.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"spec UI %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static unsigned errors;
static void log_script(void *ctx,const char *text)
{
    (void)ctx;if (strstr(text,"error:") || strstr(text,"undefined")) {errors++;fprintf(stderr,"%s\n",text);}
}
static void advance(menu *m)
{
    for (int i=0;i<60;i++) {
        menu_tick(m);menu_animate(m,1.f/60);
        plat_video_begin_frame();ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);plat_video_end_frame();
    }
}
#else
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
#endif
#include "career_vehicle.h"
static ui_widget *root_named(menu *m,const char *name)
{
    for (int i=0;i<ui_root_count(m->ui);i++)
        if (!strcmp(ui_root_name(m->ui,i),name)) return ui_root_widget(m->ui,i);
    CHECK(0);return NULL;
}
static void texts(menu *m,ui_widget *w,char *out,size_t cap)
{
    const char *text=ui_widget_text(m->ui,w);
    if (text && *text) {
        printf("Arcade %s: %s\n",w->name,text);
        strncat(out,text,cap-strlen(out)-1);strncat(out,"\n",cap-strlen(out)-1);
    }
    for (int i=0;i<w->nkids;i++) texts(m,w->kids[i],out,cap);
}
int main(int argc,char **argv)
{
    CHECK(argc>=3);char msg[256];const char *error=NULL;
#ifdef MOON_SPECS_SCREENSHOT
    CHECK(!plat_video_init(&(plat_video_desc){960,720,0,0,0}));
    SDL_HideWindow(SDL_GetWindowFromID(1));plat_video_set_view(640,480);
#endif
    gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);CHECK(gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
    menu *m=menu_open(vol,"arcade",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);
    ah_obj *game=game_object(m);ah_val option;
    CHECK(ah_get_member(m->vm,game,"option",&option));
    ah_set_member(m->vm,option.u.o,"unit_power",ah_str(m->vm,"PS"));
    ah_set_member(m->vm,option.u.o,"unit_torque",ah_str(m->vm,"NM"));
    ah_set_member(m->vm,widget_module(m,root_named(m,"GameRoot")),"current_game",ah_str(m->vm,"SingleRace"));
    ah_set_member(m->vm,widget_module(m,root_named(m,"CourseRoot")),"current_crs",ah_str(m->vm,"tsukuba"));
    ah_set_member(m->vm,widget_module(m,root_named(m,"MethodRoot")),"current_method",ah_str(m->vm,"ByMaker"));
    menu_enter(m,root_named(m,"MakerRoot")->root);advance(m);
    for (int i=0;i<100 && strcmp(menu_focus_name(m),"ford");i++) {menu_send_input(m,MENU_DOWN);advance(m);}
    CHECK(!strcmp(menu_focus_name(m),"ford"));menu_send_input(m,MENU_ACCEPT);advance(m);
    for (int i=0;i<50 && strcmp(menu_selected_car(m),argv[2]);i++) {menu_send_input(m,MENU_RIGHT);advance(m);}
    CHECK(!strcmp(menu_selected_car(m),argv[2]));
    char rendered[16384]="";texts(m,root_named(m,"CarRoot"),rendered,sizeof rendered);
    if (argc>3 && !strcmp(argv[3],"--negative")) rendered[0]=0; /* Deliberate negative: lose the UI payload. */
    CHECK(strstr(rendered,"Type:FR") && strstr(rendered,"150PS/7300rpm") && strstr(rendered,"200.02NM/4500rpm"));
    CHECK(!errors);
#ifdef MOON_SPECS_SCREENSHOT
    CHECK(argc==4);plat_video_begin_frame();ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
    int width,height;size_t size;uint8_t *pixels=plat_sdl_read_pixels(&width,&height);
    void *png=pixels?tdefl_write_image_to_png_file_in_memory(pixels,width,height,4,&size):NULL;
    FILE *file=png?fopen(argv[3],"wb"):NULL;CHECK(file);CHECK(fwrite(png,1,size,file)==size);
    fclose(file);free(pixels);mz_free(png);plat_video_end_frame();
#endif
    menu_close(m);gt4_game_set_car_factory(NULL,NULL);gt4_game_set_stats_reader(NULL,NULL);
    gt4_career_vehicle_close(factory);gt4_vol_close(vol);
#ifdef MOON_SPECS_SCREENSHOT
    plat_video_shutdown();
#endif
    puts("PASS actual Arcade CarRoot authored power/torque/RPM/drivetrain labels");return 0;
}
