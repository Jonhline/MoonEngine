/* Compare cached Arcade re-entry with the disc's own onLoad routing. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
static int page_named(menu *m, const char *name)
{
    for (int i=0;i<ui_root_count(m->ui);i++)
        if (!strcmp(ui_root_name(m->ui,i),name)) return i;
    return -1;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2);
    const char *why=NULL; char error[256];
    gt4_vol *vol=gt4_vol_open(argv[1],&why);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    menu *m=menu_open(vol,"arcade",error,sizeof error); CHECK(m);
    ah_set_log(m->vm,log_script,NULL);
    /* Rendering/resource loading and networking are inert test boundaries;
       the routing and selection code itself remains the disc bytecode. */
    ah_val namespace;
    CHECK(ah_get_member(m->vm,ah_root(m->vm),"main",&namespace));
    ah_obj *main_module=namespace.u.o;
    CHECK(ah_get_member(m->vm,main_module,"menu",&namespace));
    ah_obj *movie=submodule(m->vm,namespace.u.o,"MMovieFace");
    ah_set_member(m->vm,movie,"initialize",ah_native(m->vm,"initialize",nat_noop,NULL));
    ah_set_member(m->vm,movie,"loadIpic",ah_native(m->vm,"loadIpic",nat_noop,NULL));
    ah_obj *network=submodule(m->vm,main_module,"network");
    ah_set_member(m->vm,network,"isRTIMEInitialized",ah_native(m->vm,"isRTIMEInitialized",nat_noop,NULL));
    ah_val courses,argument=ah_str(m->vm,"tsukuba"),result;
    CHECK(ah_get_member(m->vm,game_object(m),"favorite_course",&courses));
    CHECK(ah_call_member(m->vm,courses.u.o,"append",1,&argument,&result)>0 && ah_truthy(result));
    int game=page_named(m,"GameRoot"),car=page_named(m,"CarRoot");
    CHECK(game>=0 && car>=0);
    for (int favorite=0;favorite<2;favorite++) {
        menu_enter(m,game); advance(m);
        ah_val option; CHECK(ah_get_member(m->vm,game_object(m),"option",&option));
        ah_set_member(m->vm,option.u.o,"limit_favorite_course_only",ah_int(favorite));
        ui_widget *single=child(ui_root_widget(m->ui,game),"SingleRace");
        ah_val args[]={ah_objv(m->context),ah_nil(),ah_nil()},result;
        CHECK(ah_call_member(m->vm,widget_module(m,single),"onActivate",3,args,&result)>0);
        ah_val route; CHECK(ah_get_member(m->vm,game_object(m),"major_menu_page",&route));
        const char *expected=favorite?"CourseRoot":"CategoryRoot";
        CHECK(!strcmp(ah_cstr(route),expected));
        m->pending_kind=0; /* Compare reload routing before opening the selected list. */
        m->pages[0]=car; m->depth=1;
        int target=menu_arcade_return(m);
        if(argc>2 && !strcmp(argv[2],"--negative")) target=car;
        CHECK(target==page_named(m,expected));
        m->depth=0; /* A PS2 reload has no outgoing cached page to finalize. */
        CHECK(ah_call_member(m->vm,m->project,"onLoad",1,args,&result)>0);
        CHECK(m->pending_kind && m->pending_page==target);
        m->pending_kind=0;
        if(!favorite) { menu_enter(m,target); advance(m); CHECK(menu_active_root(m)==target); }
    }
    /* A real reload retains game data and disc selections but discards
       the hidden outgoing render context and all showroom script locals. */
    menu_enter(m,game); advance(m);
    ah_val option;
    CHECK(ah_get_member(m->vm,game_object(m),"option",&option));
    ah_set_member(m->vm,option.u.o,"limit_favorite_course_only",ah_int(0));
    ui_widget *single=child(ui_root_widget(m->ui,game),"SingleRace");
    ah_val activate[]={ah_objv(m->context),ah_nil(),ah_nil()};
    CHECK(ah_call_member(m->vm,widget_module(m,single),"onActivate",3,activate,&result)>0);
    m->pending_kind=0;
    ah_set_member(m->vm,m->context,"cursor_visible",ah_int(0));
    ah_val old_car; CHECK(ah_get_member(m->vm,m->project,"CarRoot",&old_car));
    ah_set_member(m->vm,old_car.u.o,"current_car",ah_str(m->vm,"stale-showroom"));
    menu_save_capture(m);
    menu *fresh=menu_open(vol,"arcade",error,sizeof error); CHECK(fresh);
    int destination=menu_arcade_reload_state(fresh,m);
    CHECK(destination==page_named(fresh,"CategoryRoot"));
    menu_close(m); m=fresh;
    ah_set_log(m->vm,log_script,NULL);
    if(argc>2 && !strcmp(argv[2],"--cursor-negative"))
        ah_set_member(m->vm,m->context,"cursor_visible",ah_int(0));
    ah_val visible;
    CHECK(!ah_get_member(m->vm,m->context,"cursor_visible",&visible) || ah_truthy(visible));
    CHECK(ah_get_member(m->vm,m->project,"CarRoot",&old_car));
    ah_val current; CHECK(ah_get_member(m->vm,old_car.u.o,"current_car",&current));
    CHECK(!ah_cstr(current) || strcmp(ah_cstr(current),"stale-showroom"));
    menu_enter(m,destination); advance(m);
    CHECK(m->focus[destination] && available(m->focus[destination]));
    for(int step=0;step<5;step++) {
        const char *name=ui_root_name(m->ui,menu_active_root(m));
        if(!strcmp(name,"CarRoot")) break;
        menu_send_input(m,MENU_ACCEPT); advance(m);
    }
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"CarRoot"));
    ui_widget *list=child(child(ui_root_widget(m->ui,menu_active_root(m)),"CarScroll"),"SelectBox");
    if(argc>2 && !strcmp(argv[2],"--list-negative"))
        while(list->nkids) ui_widget_detach(list->kids[list->nkids-1]);
    CHECK(list->nkids>0 && m->focus[menu_active_root(m)]);
    ui_widget *tip=child(child(ui_root_widget(m->ui,menu_active_root(m)),"ToolTip"),"tip");
    char first_info[256];
    snprintf(first_info,sizeof first_info,"%s",ui_widget_text(m->ui,tip));
    CHECK(strstr(first_info,"Power:") && strstr(first_info,"Torque:") && strstr(first_info,"rpm"));
    int samples=list->nkids<4?list->nkids:4;
    for(int i=1;i<samples;i++) {
        menu_send_input(m,MENU_RIGHT); advance(m);
        CHECK(list->focus_index==i);
        ah_val root_value,ctx=ah_objv(m->context),result;
        CHECK(ah_get_member(m->vm,m->project,"CarRoot",&root_value));
        ui_widget *face=child(ui_root_widget(m->ui,menu_active_root(m)),"CarFace");
        ah_val info; CHECK(ah_get_member(m->vm,widget_module(m,face),"car_data",&info));
        char actual[256]; snprintf(actual,sizeof actual,"%s",ui_widget_text(m->ui,tip));
        ah_val args[]={ctx,info};
        CHECK(ah_call_member(m->vm,root_value.u.o,"set_car_info",2,args,&result)>0);
        if(argc>2 && !strcmp(argv[2],"--info-negative")) actual[0]=0;
        CHECK(!strcmp(actual,ui_widget_text(m->ui,tip)) && strstr(actual,"Torque:"));
    }
    CHECK(samples>=3);
    menu_send_input(m,MENU_BACK); advance(m);
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"MakerRoot"));
    menu_send_input(m,MENU_RIGHT); advance(m);
    menu_send_input(m,MENU_ACCEPT); advance(m);
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"CarRoot"));
    if(argc>2 && !strcmp(argv[2],"--index-negative")) list->focus_index=2;
    CHECK(list->focus_index==0 && list->nkids>0);
    CHECK(strstr(ui_widget_text(m->ui,tip),"Power:") && strstr(ui_widget_text(m->ui,tip),"Torque:"));
    /* The following original onLoad reference needs inert render/network boundaries. */
    CHECK(ah_get_member(m->vm,ah_root(m->vm),"main",&namespace)); main_module=namespace.u.o;
    CHECK(ah_get_member(m->vm,main_module,"menu",&namespace));
    movie=submodule(m->vm,namespace.u.o,"MMovieFace");
    ah_set_member(m->vm,movie,"initialize",ah_native(m->vm,"initialize",nat_noop,NULL));
    ah_set_member(m->vm,movie,"loadIpic",ah_native(m->vm,"loadIpic",nat_noop,NULL));
    network=submodule(m->vm,main_module,"network");
    ah_set_member(m->vm,network,"isRTIMEInitialized",ah_native(m->vm,"isRTIMEInitialized",nat_noop,NULL));
    /* No selected mode follows the original TopRoot fallback, even if a
       stale destination remains in the profile. */
    argument=ah_objv(m->context);
    CHECK(ah_call_member(m->vm,game_object(m),"menuClearStack",0,NULL,&result)>0);
    ah_set_member(m->vm,game_object(m),"major_menu_page",ah_str(m->vm,"CategoryRoot"));
    int fallback=menu_arcade_return(m);
    CHECK(fallback==page_named(m,"TopRoot"));
    m->depth=0;
    CHECK(ah_call_member(m->vm,m->project,"onLoad",1,&argument,&result)>0);
    CHECK(m->pending_kind && m->pending_page==fallback);
    CHECK(menu_arcade_return(NULL)==-1);
    CHECK(!errors);
    menu_close(m); gt4_vol_close(vol); gt4_save_shutdown();
    puts("PASS Arcade cached return matches authored normal/favorites-only reload");
    return 0;
}
