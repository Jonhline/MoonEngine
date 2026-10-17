/* Original EventInfoDialog scripts and RT03 texts; renderer recording is
   headless. No career result or user's profile is changed. */
#define plat_draw_quad4 regulation_quad4
#define plat_set_clip regulation_fixture_clip
#define main purchase_fixture_main
#include "purchase_menu.c"
#undef main
#undef plat_draw_quad4
#undef plat_set_clip
static float regulation_clip[4];
void plat_set_clip(float x,float y,float w,float h)
{ regulation_clip[0]=x;regulation_clip[1]=y;regulation_clip[2]=w;regulation_clip[3]=h;regulation_fixture_clip(x,y,w,h); }
static float regulation_background_height;
static float regulation_last_background_height;
static float regulation_first_clip[4];
void plat_draw_quad4(const plat_texture *t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4])
{
    if(!t && w==100 && c[0][0]==3 && c[0][1]==18 && c[0][2]==36) {
        if(!regulation_background_height) { regulation_background_height=h;memcpy(regulation_first_clip,regulation_clip,sizeof regulation_first_clip); }
        regulation_last_background_height=h;
    }
    regulation_quad4(t,x,y,w,h,u,v,a,b,c);
}

#ifndef REGULATION_ENTRY
#define REGULATION_ENTRY main
#endif
int REGULATION_ENTRY(int argc, char **argv)
{
    CHECK(argc==2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory = gt4_career_vehicle_open(argv[1]);
    CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, factory);
    gt4_save_init("");
    char error[256];
    const char *codes[] = {"am_sunday_0000", "am_ff_0000", "am_ff_0000", "am_pickup_0000"};
    const char *cars[] = {"ka_01", "ka_01", "beat_f_92", "ka_01"};
    const char *headers[] = {"Game Mode", "Drivetrain", "Tires",
                             "Cars\r\nPermitted:", "Required\r\nLicense"};
    const char *descriptions[] = {"Single Race", "FF cars only",
                                  "Standard or Sports Tires required", "All cars permitted",
                                  "None"};
    for (unsigned test = 0; test < 4; test++) {
        menu *m = menu_open(vol, "gtmode", error, sizeof error);
        CHECK(m);
        menu_set_car_loader(m, transaction_model_loader, NULL);
        ah_obj *garage = get(m->vm, game_object(m), "garage").u.o;
        ah_val car[] = {ah_str(m->vm, cars[test]), ah_int(0)}, result, index = ah_int(0);
        CHECK(ah_call_member(m->vm,garage,"addNewCar",2,car,&result)>=0);
        CHECK(ah_call_member(m->vm,garage,"getCount",0,NULL,&result)>=0);
        index = ah_int((int)ah_num(result) - 1);
        CHECK(ah_call_member(m->vm,garage,"setRidingCar",1,&index,&result)>=0);
        menu_save_capture(m);
        ah_obj *page = get(m->vm, m->project, "EventInfoDialog").u.o;
        ah_obj *event = ah_module_new(m->vm, "fixture_event", NULL, NULL, NULL);
        ah_set_member(m->vm, event, "race_code", ah_str(m->vm, codes[test]));
        ah_set_member(m->vm, page, "event", ah_objv(event));
        script_errors = 0;
        ah_set_log(m->vm, log_script, NULL);
        CHECK(enter_named(m,"EventInfoDialog"));
        ticks(m);
        regulation_background_height=0;
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
        CHECK(!script_errors);
        ah_val riding = get(m->vm, get(m->vm, game_object(m), "garage").u.o, "riding_car"), drive;
        CHECK(!strcmp(car_label_of(m->vm,riding),cars[test]));
        CHECK(ah_call_member(m->vm,riding.u.o,"getDTrainType",0,NULL,&drive)>=0);
        CHECK(!strcmp(ah_cstr(drive),test==2?"MR":"FF"));
        ui_widget *pane = kid(ui_root_widget(m->ui, menu_active_root(m)), "Pane");
        ui_widget *grid = kid(kid(kid(kid(pane, "VBox"), "Regulation"), "clip"), "space");
        CHECK(grid);
        /* Full geometry, independently quantized GS origin/extent. */
        float padding=(float)mp_number(grid->kids[0]->src,"pad_bottom",0);
        CHECK(padding==4);
        if(getenv("GT4_REGULATION_DIVIDERS_NEGATIVE"))
            regulation_background_height+=2;
        CHECK(regulation_background_height==grid->kids[0]->h);
        CHECK(regulation_last_background_height==grid->kids[grid->nkids-2]->h);
        CHECK(regulation_first_clip[3] <= regulation_background_height);
        float expected_height=(int)(regulation_background_height*(448.f/480.f)*.5f)/(224.f/480.f);
        CHECK(regulation_first_clip[3]<=expected_height+.001f);
        float field_bottom=(regulation_first_clip[1]+regulation_first_clip[3])*(224.f/480.f);
        CHECK(fabsf(field_bottom-roundf(field_bottom))<.001f);
        if(test==3) {
            ui_widget *clip=grid->parent;
            CHECK(grid->h > clip->h);
            float grid_height=grid->h;
            regulation_background_height=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
            CHECK(regulation_background_height==48);
            ui_menu_readability(m->ui,1);
            regulation_background_height=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
            CHECK(regulation_background_height==48 && grid->h==grid_height);
            ui_menu_readability(m->ui,0);
            regulation_background_height=0;ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
            CHECK(regulation_background_height==48);
            menu_send_input(m,MENU_DOWN);ticks(m);
            CHECK(fabsf(grid->y - (clip->h-grid->h)) < .01f);
            menu_send_input(m,MENU_UP);ticks(m);
            CHECK(fabsf(grid->y) < .01f);
            ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
            float px=clip->pointer_rect[0]+clip->pointer_rect[2]/2;
            float py=clip->pointer_rect[1]+clip->pointer_rect[3]/2;
            CHECK(!menu_pointer(m,0,px,py,0,-1));ticks(m);
            CHECK(fabsf(grid->y) < .01f);
            CHECK(menu_pointer(m,1,px,py,0,-1));ticks(m);
            CHECK(fabsf(grid->y-(clip->h-grid->h)) < .01f && !grid->scroll_set);
            CHECK(menu_pointer(m,1,px,py,0,1));ticks(m);
            CHECK(fabsf(grid->y) < .01f);
            menu_close(m);continue;
        }
        CHECK(grid->nkids==(test?10:8));
        CHECK(!strcmp(ui_widget_text(m->ui,kid(kid(pane,"Title"),"label")),test?"FF Challenge":"Sunday Cup"));
        for (int i = 0; i < grid->nkids; i++) {
            int row = i / 2;
            if (!test && row)
                row++;
            ui_widget *cell = grid->kids[i], *text = kid(cell, "text");
            CHECK(text);
            if (i % 2 && !text->visible)
                text = kid(cell, "textv");
            CHECK(text && text->visible);
            const char *actual = ui_widget_text(m->ui, text);
            if (getenv("GT4_REGULATION_TEXT_NEGATIVE") && !test && i == 1)
                actual = "Championship";
            CHECK(!strcmp(actual,i%2?descriptions[row]:headers[row]));
            if (test == 2 && i == 3)
                CHECK(!strcmp(text->name,"textv"));
            if (test == 1 && i == 3)
                CHECK(!strcmp(text->name,"text"));
        }
        printf("PASS %s / %s: original labels, descriptions and drivetrain violation text\n",
               codes[test], cars[test]);
        menu_close(m);
    }
    gt4_save_shutdown();
    gt4_career_vehicle_close(factory);
    gt4_vol_close(vol);
    return 0;
}
