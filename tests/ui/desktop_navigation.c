#define plat_draw_quad4 fixture_draw_quad4
#define plat_draw_rotated fixture_draw_rotated
#define main setting_fixture_main
#include "../gtmode/setting_menu.c"
#undef main
#undef plat_draw_rotated
#undef plat_draw_quad4
#include "race_pause.h"
void plat_video_set_view(float w,float h) {(void)w;(void)h;}
static float tip[2];
static int hover_faces, gold_overlays;
static unsigned quad_hash;
static int check_thumbnails, course_gradient, gradient_bad;
static float thumbnail_width;
static int shine_draws;
static float shine_y;
void plat_draw_quad4(const plat_texture *t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4])
{
    if(check_thumbnails && t && t->w==50 && t->h==32) thumbnail_width=fmaxf(thumbnail_width,w);
    if(!t && c[0][0]==143 && c[0][1]==9 && c[0][2]==9) {
        course_gradient++;
        if(getenv("MOON_GRADIENT_NEGATIVE")) gradient_bad++;
        if(c[2][0]!=143 || c[2][3]!=c[0][3] || c[1][3] || c[3][3]) gradient_bad++;
    }
    if(!t && w<=20 && h<20 && c[0][0]==255 && c[2][0]==255 &&
       c[0][3]!=c[2][3] && c[0][3]<=44 && c[2][3]<=44) {
        shine_draws++;shine_y=y;
    }
    float coords[]={x,y,w,h};
    for(int k=0;k<4;k++) {unsigned bits;memcpy(&bits,&coords[k],4);quad_hash=(quad_hash^bits)*16777619u;}
    if(!t && c[0][0]==242 && c[0][1]==177 && c[0][2]==44 && c[0][3]>0) gold_overlays++;
    if(x==8 && y==8 && w==80 && h==40 && c[0][0]!=c[0][1]) hover_faces++;
    fixture_draw_quad4(t,x,y,w,h,u,v,a,b,c);
}
void plat_draw_rotated(const plat_texture *t,float x,float y,float w,float h,float cx,float cy,float a,const uint8_t c[4])
{(void)t;(void)x;(void)y;(void)w;(void)h;(void)a;(void)c;tip[0]=cx;tip[1]=cy;}
static void paint(menu *m)
{menu_animate(m,1.f);ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);}
static ui_widget *space_of(ui_widget *w)
{
    ui_widget *clip=child(w,"clip"), *space=clip?child(clip,"space"):NULL;
    if(space && space->h>clip->h) return space;
    for(int i=0;i<w->nkids;i++) {space=space_of(w->kids[i]);if(space)return space;}
    return NULL;
}
static unsigned image_paths(const ui_widget *w)
{
    unsigned h=2166136261u;
    if(w->image_path) for(const char *p=w->image_path;*p;p++) h=(h^(unsigned char)*p)*16777619u;
    for(int i=0;i<w->nkids;i++) h=(h^image_paths(w->kids[i]))*16777619u;
    return h;
}
static int preview_model(menu *m,const ui_widget *w,void *user)
{
    (void)m;(void)user;
    return w && w->car_code;
}
static int activated;
static menu *destination;
static int activate(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret)
{
    (void)vm;(void)self;(void)argc;(void)argv;
    menu *m=data;
    ui_cursor_transfer(destination->ui,m->ui);
    paint(destination);
    activated++;
    *ret=ah_int(2);return 0;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2);
    gt4_vol *vol=gt4_vol_open(argv[1],NULL);
    CHECK(vol);
    char err[256];
    menu *m=menu_open(vol,"option",err,sizeof err);
    destination=menu_open(vol,"option",err,sizeof err);
    if(m) ah_set_log(m->vm,log_script,NULL);
    if(destination) ah_set_log(destination->vm,log_script,NULL);
    CHECK(m && destination && page(m,"OptionRoot")>=0 && page(destination,"UnitRoot")>=0);
    ticks(m);ticks(destination);paint(m);
    ui_widget *root=ui_root_widget(m->ui,menu_active_root(m));
    ui_widget *space=space_of(root);
    CHECK(space);
    ui_widget *clip=space->parent;
    float x=clip->pointer_rect[0]+clip->w*.5f,y=clip->pointer_rect[1]+clip->h*.5f;
    ui_widget *tiles[512];int nt=0;candidates(root,tiles,&nt);
    CHECK(nt>5);
    int exit_feedback=0, tile_feedback=0;
    ui_pointer_feedback(m->ui,1);
    for(int i=0;i<nt;i++) {
        ui_widget *tile=tiles[i];float *r=tile->pointer_rect;
        if(r[2]<=0 || r[3]<=0) continue;
        if(menu_pointer(m,1,r[0]+r[2]/2,r[1]+r[3]/2,0,0)) {
            menu_animate(m,.08f);
            CHECK(tile->pointer_light>.5f);
            ui_pointer_feedback(m->ui,0);quad_hash=0;paint(m);unsigned plain=quad_hash;
            ui_pointer_feedback(m->ui,1);quad_hash=0;gold_overlays=0;paint(m);
            CHECK(!gold_overlays);
            if(plain!=quad_hash) {
                tile_feedback++;
                if(r[0]>500 && r[1]>300) exit_feedback=1;
            }
        }
    }
    CHECK(exit_feedback && tile_feedback>1);
    ui_pointer_feedback(m->ui,0);gold_overlays=0;menu_pointer(m,0,0,0,0,0);paint(m);CHECK(!gold_overlays);
    ui_widget *before=m->focus[menu_active_root(m)];
    float old=space->scroll_set?space->scroll_ty:space->y;
    menu_pointer(m,0,x,y,0,-2);
    CHECK((space->scroll_set?space->scroll_ty:space->y)==old);
    menu_pointer(m,1,x,y,0,-2);
    CHECK(space->scroll_ty<old && m->focus[menu_active_root(m)]==before);
    float target=space->scroll_ty;
    menu_pointer(m,1,x,y,0,0);paint(m);
    CHECK(space->scroll_ty==target && m->focus[menu_active_root(m)]==before);
    menu_pointer(m,1,x,y,0,1000);
    CHECK(space->scroll_ty==0);
    /* Full painted button (80x40), intentionally larger than client (10x10). */
    ui_widget *button=ui_widget_from_text(m->ui,menu_active_root(m),
       "Button { name string {\"DesktopTest\"} geometry rectangle {8 8 10 10} children [2] { ColorFace { name string {\"bg\"} geometry rectangle {0 0 80 40} color [1] { RGBA {128 128 128 128} } } ColorFace { name string {\"hover\"} geometry rectangle {0 0 80 40} color [1] { RGBA {11 22 33 128} } visible_compare digit {4} visible_condition digit {4} } } }");
    CHECK(button);ui_widget_append(root,button);
    menu_native_method(m,button,"onActivate",activate,m);
    paint(m);
    CHECK(ui_pointer_contains(m->ui,button,85,45));
    hover_faces=0;
    menu_pointer(m,1,85,45,0,0);
    paint(m);
    CHECK(hover_faces>0);
    CHECK(m->focus[menu_active_root(m)]==before);
    if(argc>2) button->visible=0;
    menu_pointer(m,1,85,45,1,0);
    CHECK(activated==1 && tip[0]==85 && tip[1]==45);
    /* Prompt transformation is presentation only and exactly reversible. */
    ui_widget *label=ui_widget_from_text(m->ui,menu_active_root(m),
      "TextFace { text string {\"%Bf Return %Ba Transfer game data\"} }");
    CHECK(label);
    ui_input_hints(m->ui,0);
    CHECK(!strcmp(ui_widget_text(m->ui,label),"%Bf Return %Ba Transfer game data"));
    ui_input_hints(m->ui,1);
    CHECK(!strcmp(ui_widget_text(m->ui,label),"[Insert] Return [Pause] Transfer game data"));
    ui_input_hints(m->ui,2);
    CHECK(!strcmp(ui_widget_text(m->ui,label),"[X] Return [Menu] Transfer game data"));
    ui_input_hints(m->ui,0);
    CHECK(!strcmp(ui_widget_text(m->ui,label),ui_widget_source_text(m->ui,label)));
    menu_close(m);menu_close(destination);
    CHECK(gt4_game_load_disc(argv[1]));gt4_save_init("");
    m=menu_open(vol,"gtmode",err,sizeof err);
    CHECK(m && page(m,"TopRoot")>=0);ui_pointer_feedback(m->ui,1);ticks(m);paint(m);
    root=ui_root_widget(m->ui,menu_active_root(m));
    space=space_of(root);CHECK(space);clip=space->parent;
    x=clip->pointer_rect[0]+clip->w-1;y=clip->pointer_rect[1]+clip->h*.5f;
    before=m->focus[menu_active_root(m)];
    menu_pointer(m,1,x,y,0,0);
    old=space->scroll_set?space->scroll_tx:space->x;
    menu_edge_pan(m,0,1.f/60);
    CHECK((space->scroll_set?space->scroll_tx:space->x)==old);
    menu_edge_pan(m,1,1.f/60);
    CHECK(space->scroll_tx<old && m->focus[menu_active_root(m)]==before);
    CHECK(space->scroll_tx>=clip->w-space->w);
    /* Footer text is outside map hit bounds but still drives downward pan. */
    float footer_y=root->h-1;
    CHECK(footer_y>=clip->pointer_rect[1]+clip->pointer_rect[3]);
    space->scroll_ty=(clip->h-space->h)*.5f;
    space->scroll_set=1;float prior_y=space->scroll_ty;
    menu_pointer(m,1,root->w*.5f,footer_y,0,0);
    menu_edge_pan(m,0,1.f/60);CHECK(space->scroll_ty==prior_y);
    menu_edge_pan(m,1,1.f/60);
    CHECK(space->scroll_ty<prior_y && space->scroll_ty>=clip->h-space->h);
    CHECK(m->focus[menu_active_root(m)]==before);
    int map_root=menu_active_root(m);
    menu_pointer(m,1,root->w*.5f,footer_y,1,0);
    CHECK(menu_active_root(m)==map_root && m->focus[map_root]==before && !m->next_project[0]);
    prior_y=space->scroll_ty;
    menu_pointer(m,1,-5,footer_y,0,0);menu_edge_pan(m,1,1.f/60);
    CHECK(space->scroll_ty==prior_y);
    ticks(m);paint(m);

    ui_widget *icons[512];int icon_count=0;
    candidates(root,icons,&icon_count);
    int hovered_icon=0;
    for(int i=0;i<icon_count && !hovered_icon;i++) {
        ui_widget *model=child(icons[i],"bg");
        float *r=icons[i]->pointer_rect;
        if(!model || strcmp(model->type,"ModelFace") || r[2]<=0 || r[3]<=0) continue;
        float px=r[0]+r[2]/2,py=r[1]+r[3]/2;
        menu_pointer(m,1,px,py,0,0);
        CHECK(m->focus[menu_active_root(m)]==before);
        printf("map hover %s velocity=%g,%g,%g mode=%d\n",icons[i]->name,
            model->model_velocity[0],model->model_velocity[1],model->model_velocity[2],model->model_target_mode);
        CHECK(model->model_velocity[0]!=0 || model->model_velocity[1]!=0 || model->model_velocity[2]!=0);
        ticks(m);gold_overlays=0;paint(m);CHECK(!gold_overlays);
        hovered_icon=1;
    }
    CHECK(hovered_icon);
    menu_close(m);
    /* Original BuyCarRoot.CarScroll callbacks: hover previews, click activates. */
    m=menu_open(vol,"gtmode",err,sizeof err);CHECK(m);
    menu_set_car_loader(m,preview_model,NULL);
    ah_obj *dealer=get(m->vm,m->project,"BuyCarRoot").u.o;
    ah_obj *catalog=ah_array_new(m->vm);
    ah_array_push(m->vm,catalog,ah_str(m->vm,"ka_01"));
    ah_array_push(m->vm,catalog,ah_str(m->vm,"svt_f150_lightning_03"));
    ah_val open_args[]={ah_objv(m->context),ah_objv(catalog),
        get(m->vm,m->project,"DealerRoot"),ah_str(m->vm,"ford"),ah_str(m->vm,"new")}, result;
    CHECK(ah_call_member(m->vm,dealer,"open",5,open_args,&result)>=0);ticks(m);paint(m);
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"BuyCarRoot"));
    ui_widget *dealer_root=ui_root_widget(m->ui,menu_active_root(m));
    ui_widget *dealer_list=child(child(dealer_root,"CarScroll"),"SelectBox");
    ui_widget *dealer_face=child(dealer_root,"CarFace");
    CHECK(dealer_list && dealer_list->nkids==2 && dealer_face);
    check_thumbnails=1;thumbnail_width=0;paint(m);
    CHECK(fabsf(thumbnail_width-70)<.01f);
    ui_menu_readability(m->ui,1);thumbnail_width=0;paint(m);
    CHECK(fabsf(thumbnail_width-56)<.01f && dealer_list->kids[0]->w==50);
    ui_menu_readability(m->ui,0);thumbnail_width=0;paint(m);
    CHECK(fabsf(thumbnail_width-70)<.01f);check_thumbnails=0;
    float *dr=dealer_list->kids[1]->pointer_rect;
    float dx=dr[0]+dr[2]/2,dy=dr[1]+dr[3]/2;
    CHECK(!menu_pointer(m,0,dx,dy,0,0));
    CHECK(dealer_list->focus_index==0);
    float dealer_offset=dealer_list->strip_offset;
    menu_pointer(m,1,dx,dy,0,0);
    CHECK(!strcmp(ah_cstr(get(m->vm,dealer,"current_car")),"svt_f150_lightning_03"));
    CHECK(dealer_list->focus_index==1 && dealer_list->desktop_scrolled);
    CHECK(dealer_face->car_pose_set && dealer_face->car_motion==1);
    CHECK(dealer_face->car_position[2]==8.5f && dealer_face->car_target_position[2]==0);
    CHECK(dealer_face->car_position_ratio==.025f);
    CHECK(dealer_face->car_model_opacity==0);
    float zoom_before=dealer_list->kids[1]->strip_zoom;
    menu_animate(m,1.f/60);
    CHECK(dealer_list->kids[1]->strip_zoom>zoom_before && dealer_list->kids[1]->strip_zoom<1.4f);
    ticks(m);paint(m);
    CHECK(dealer_list->strip_offset==dealer_offset);
    CHECK(dealer_face->car_position[2]>0 && dealer_face->car_position[2]<8.5f);
    CHECK(dealer_face->car_model_opacity>0 && dealer_face->car_model_opacity<1);
    dr=dealer_list->kids[1]->pointer_rect;
    menu_pointer(m,1,dr[0]+dr[2]/2,dr[1]+dr[3]/2,1,0);ticks(m);
    CHECK(!strcmp(ah_cstr(get(m->vm,dealer,"current_car")),"svt_f150_lightning_03"));
    CHECK(child(child(dealer_root,"ColScroll"),"SelectBox")->nkids>0);
    paint(m);
    ui_widget *paint_list=child(child(dealer_root,"ColScroll"),"SelectBox");
    CHECK(paint_list->nkids>1);
    float paint_offset=paint_list->strip_offset;
    ui_pointer_feedback(m->ui,1);
    int color_order[]={1,0,1,0};
    for(int ci=0;ci<4;ci++) {
        if(ci==3) menu_pointer(m,1,-1,-1,0,0);
        ui_widget *swatch=paint_list->kids[color_order[ci]];
        float *r=swatch->pointer_rect;
        menu_pointer(m,1,r[0]+r[2]/2,r[1]+r[3]/2,0,0);
        CHECK(paint_list->focus_index==color_order[ci]);
        CHECK(dealer_face->car_color_index==color_order[ci]);
        ticks(m);paint(m);
        CHECK(paint_list->strip_offset==paint_offset);
    }
    shine_draws=0;paint(m);CHECK(shine_draws>0);
    float old_shine_y=shine_y;
    shine_draws=0;paint(m);CHECK(shine_draws>0 && shine_y!=old_shine_y);
    ui_pointer_feedback(m->ui,0);shine_draws=0;paint(m);CHECK(!shine_draws);
    ui_pointer_feedback(m->ui,1);shine_draws=0;paint(m);CHECK(shine_draws>0);
    int color_before=dealer_face->car_color_index;
    ui_widget *second=paint_list->kids[1];float *sr=second->pointer_rect;
    CHECK(!menu_pointer(m,0,sr[0]+sr[2]/2,sr[1]+sr[3]/2,0,0));
    CHECK(dealer_face->car_color_index==color_before);

    /* DealerRoot.onInitialize instructions 54..63 use this exact disc image. */
    size_t logo_bytes=0;
    uint8_t *logo_data=gt4_vol_read(vol,"/piece/tuner_logo/ford.img",&logo_bytes);
    tex1_image *logos=NULL;int logo_count=logo_data?tex1_decode(logo_data,logo_bytes,&logos):0;
    CHECK(logo_count==1 && logos[0].width==378 && logos[0].height==135);
    const uint8_t *plate=logos[0].rgba+((10*378+100)*4);
    CHECK(plate[0]==0 && plate[1]==0 && plate[2]==0 && plate[3]==255);
    CHECK(logos[0].rgba[3]==0);
    tex1_free(logos,logo_count);free(logo_data);
    menu_close(m);
    /* Real Arcade Ford catalog: hover previews without scrolling/selection. */
    m=menu_open(vol,"arcade",err,sizeof err);
    CHECK(m && page(m,"MakerRoot")>=0);ticks(m);paint(m);
    ui_widget *makers[512];int makers_n=0;
    candidates(ui_root_widget(m->ui,menu_active_root(m)),makers,&makers_n);
    ui_widget *ford=NULL;
    for(int i=0;i<makers_n;i++) if(!strcmp(makers[i]->name,"ford")) ford=makers[i];
    CHECK(ford);set_focus(m,ford);menu_send_input(m,MENU_ACCEPT);ticks(m);paint(m);
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"CarRoot"));
    char car_before[96];snprintf(car_before,sizeof car_before,"%s",menu_selected_car(m));
    ui_widget *cars[512];int cars_n=0;
    candidates(ui_root_widget(m->ui,menu_active_root(m)),cars,&cars_n);
    int previewed=0;
    /* Controller candidates expose ListBox itself; mouse targets its cards. */
    if(cars_n==1 && !strcmp(cars[0]->type,"ListBox")) {
        ui_widget *list=cars[0];cars_n=0;
        for(int i=0;i<list->nkids && cars_n<512;i++) cars[cars_n++]=list->kids[i];
    }
    for(int i=0;i<cars_n && !previewed;i++) {
        ui_widget *item=cars[i],*list=item->parent;
        float *r=item->pointer_rect;
        if(!list || (strcmp(list->type,"SelectBox") && strcmp(list->type,"ListBox")) || sibling_index(item)==list->focus_index || item==m->focus[menu_active_root(m)] || r[2]<=0 || r[3]<=0) continue;
        int index=list->focus_index;
        float offset=list->strip_offset;
        menu_pointer(m,1,r[0]+r[2]/2,r[1]+r[3]/2,0,0);
        ticks(m);
        if(strcmp(menu_selected_car(m),car_before)) {
            CHECK(list->focus_index==index && list->strip_offset==offset);
            previewed=1;
        }
    }
    CHECK(previewed);
    menu_close(m);
    /* P: retain original labels/actions with five visible vertical entries. */
    m=menu_open(vol,"arcade",err,sizeof err);CHECK(m && page(m,"TopRoot")>=0);
    ticks(m);paint(m);
    ui_widget *main_list=child(child(ui_root_widget(m->ui,menu_active_root(m)),"StartScroll"),"SelectBox");
    CHECK(main_list && main_list->nkids>=8);
    ui_main_menu_layout(m->ui,1);paint(m);
    for(int i=0;i<5;i++) {
        CHECK(ui_pointer_contains(m->ui,main_list->kids[i],165,306+i*25.f));
        CHECK(ui_pointer_contains(m->ui,main_list->kids[i],475,320+i*25.f));
        menu_pointer(m,1,165,306+i*25.f,0,0);
        CHECK(m->pointer_hover==main_list->kids[i]);
    }
    /* Whole main-menu line, not just the middle word of its label. */
    CHECK(menu_pointer(m,1,60,335,0,0) && m->pointer_hover==main_list->kids[1]);
    CHECK(menu_pointer(m,1,580,335,0,0) && m->pointer_hover==main_list->kids[1]);
    ui_pointer_feedback(m->ui,1);gold_overlays=0;paint(m);CHECK(!gold_overlays);
    CHECK(!menu_pointer(m,1,620,335,0,0));
    int selected=main_list->focus_index;
    CHECK(ui_pointer_scroll(m->ui,menu_active_root(m),320,350,-3));paint(m);
    CHECK(main_list->focus_index==selected);
    CHECK(ui_pointer_contains(m->ui,main_list->kids[7],320,410));
    ui_main_menu_layout(m->ui,0);paint(m);
    CHECK(!ui_pointer_contains(m->ui,main_list->kids[7],320,410));
    ui_main_menu_layout(m->ui,1);paint(m);
    menu_pointer(m,1,320,362,1,0);ticks(m);
    CHECK(!strcmp(m->next_project,"option"));
    menu_close(m);
    /* Authored course preview follows hover, not the old focused card. */
    m=menu_open(vol,"arcade",err,sizeof err);CHECK(m);
    menu_preview_courses(m,1);CHECK(page(m,"TopRoot")>=0);ticks(m);
    for(int i=0;i<6 && strcmp(ui_root_name(m->ui,menu_active_root(m)),"CourseRoot");i++) {
        menu_send_input(m,MENU_ACCEPT);ticks(m);
    }
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"CourseRoot"));paint(m);
    char course_before[96];snprintf(course_before,sizeof course_before,"%s",menu_selected_course(m));
    ui_widget *courses[512];int course_n=0;
    candidates(ui_root_widget(m->ui,menu_active_root(m)),courses,&course_n);
    ui_widget *course_list=child(child(ui_root_widget(m->ui,menu_active_root(m)),"CrsScroll"),"SelectBox");
    CHECK(course_list);
    unsigned old_images=image_paths(ui_root_widget(m->ui,menu_active_root(m)));
    int original_index=course_list->focus_index;previewed=0;
    for(int i=0;i<course_list->nkids && !previewed;i++) {
        ui_widget *item=course_list->kids[i];float *r=item->pointer_rect;
        if(i==original_index || r[2]<=0 || r[3]<=0) continue;
        menu_pointer(m,1,r[0]+r[2]/2,r[1]+r[3]/2,0,0);ticks(m);
        previewed=strcmp(menu_selected_course(m),course_before)!=0;
    }
    CHECK(previewed && course_list->focus_index==original_index);
    CHECK(image_paths(ui_root_widget(m->ui,menu_active_root(m)))!=old_images);
    CHECK(course_gradient && !gradient_bad);
    ui_widget *course_bar=child(course_list->parent,"bg"),*course_thumb=child(course_list->parent,"pinch");
    CHECK(course_bar && course_thumb);
    float thumb_start=course_thumb->x;
    set_focus(m,course_list->kids[course_list->nkids-1]);ticks(m);paint(m);
    CHECK(course_thumb->x>thumb_start);
    CHECK(course_thumb->x+course_thumb->w<=course_bar->x+course_bar->w+.01f);
    set_focus(m,course_list->kids[0]);ticks(m);paint(m);
    CHECK(course_thumb->x<=thumb_start+.01f);
    menu_close(m);
    race_pause *pause=race_pause_open(vol,0,0,0,NULL,NULL);
    CHECK(pause);race_pause_update(pause,1);
    CHECK(!race_pause_pointer(pause,0,320,240,1) && race_pause_state(pause)==RACE_PAUSE_OPEN);
    int found=0;
    for(int py=0;py<480 && !found;py+=8)
        for(int px=0;px<640 && !found;px+=8)
            if(race_pause_pointer(pause,1,(float)px,(float)py,0)) {
                CHECK(race_pause_state(pause)==RACE_PAUSE_OPEN);
                race_pause_draw(pause);
                race_pause_pointer(pause,1,(float)px,(float)py,1);
                race_pause_update(pause,1);
                CHECK(race_pause_state(pause)!=RACE_PAUSE_OPEN);
                found=1;
            }
    CHECK(found);race_pause_close(pause);
    m=menu_open(vol,"option",err,sizeof err);CHECK(m && page(m,"CourseRoot")>=0);
    ticks(m);paint(m);
    ui_widget *course_rows[512];int row_count=0;
    candidates(ui_root_widget(m->ui,menu_active_root(m)),course_rows,&row_count);
    int list_checked=0;
    for(int i=0;i<row_count;i++) {
        ui_widget *list=course_rows[i]->parent;
        if(!list || strcmp(list->type,"SelectBox") || list->nkids<15) continue;
        float *r=list->pointer_rect;
        for(int j=0;j<list->nkids;j++) {
            ui_widget *row=list->kids[j];
            if(row->drawn_tick!=ui_draw_tick(m->ui) || row->pointer_rect[3]<=0) continue;
            CHECK(row->pointer_rect[1]+row->pointer_rect[3]<=r[1]+r[3]+.01f);
        }
        CHECK(ui_pointer_scroll(m->ui,menu_active_root(m),r[0]+r[2]/2,r[1]+r[3]/2,-100));
        ticks(m);paint(m);
        CHECK(list->kids[list->nkids-1]->drawn_tick==ui_draw_tick(m->ui));
        CHECK(list->kids[list->nkids-1]->pointer_rect[3]>=list->kids[list->nkids-1]->h-.01f);
        list_checked=1;break;
    }
    CHECK(list_checked);menu_close(m);
    CHECK(!errors);
    gt4_vol_close(vol);gt4_save_shutdown();
    puts("Desktop adaptations: content wheel, hover without scrolling, full button, popup pointer, prompts and map pan passed.");
    return 0;
}
