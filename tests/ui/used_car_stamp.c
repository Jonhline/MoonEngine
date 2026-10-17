/* Exercise the original UsedCarRoot resource and its sold-out label. */
#define main purchase_fixture_main
#define plat_draw_rotated_uv fixture_rotated_uv
#include "../gtmode/purchase_menu.c"
#undef plat_draw_rotated_uv
#undef main

static unsigned stamps;
static float pivot_x, pivot_y;
static int bad_rotation;
void plat_draw_rotated_uv(const plat_texture *t, float x, float y, float w, float h,
                          float cx, float cy, float angle, float u0, float v0,
                          float u1, float v1, const uint8_t c[4])
{
    if (c[0] == 230 && c[1] == 134 && c[2] == 0) {
        if (stamps && (cx != pivot_x || cy != pivot_y)) bad_rotation++;
        if (angle != -15) bad_rotation++;
        pivot_x = cx; pivot_y = cy;
        stamps++;
    }
    plat_draw_glyph(t,x,y,w,h,y,0,u0,v0,u1,v1,c);
}

int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    const char *err = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory = gt4_career_vehicle_open(argv[1]);
    CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats, factory);
    gt4_save_init("");
    char msg[256];
    menu *m = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(m);
    menu_enter(m,0); ticks(m);
    ah_obj *page = get(m->vm,m->project,"UsedCarRoot").u.o;
    ah_set_member(m->vm,page,"current_region",ah_str(m->vm,"US"));
    CHECK(enter_named(m,"UsedCarRoot")); ticks(m);
    ui_widget *root = ui_root_widget(m->ui,menu_active_root(m));
    ui_widget *list = kid(kid(root,"ScrollBox"),"SelectBox");
    CHECK(list && list->nkids);
    ui_widget *stamp = NULL;
    for (int i=0;i<list->nkids;i++) {
        ui_widget *label=kid(list->kids[i],"SoldOut");
        CHECK(label);
        label->visible=i==0;
        if (!i) stamp=label;
    }
    CHECK(stamp && stamp->w==64 && stamp->h==56);
    CHECK(mp_number(stamp->src,"rotate",0)==-15);
    CHECK(mp_number(stamp->src,"multiline",0)==1);
    const char *source=ui_widget_source_text(m->ui,stamp);
    CHECK(source && strstr(source,"SOLD") && !strchr(source,'\n'));
    if (argc>2) ui_text_wrap(m->ui,stamp,stamp->w); /* reproduce old bug */
    ui_draw_root(m->ui,menu_active_root(m),NULL,NULL); /* warm font/cache */
    clock_t start=clock();
    for (int i=0;i<120;i++) {
        stamps=0; bad_rotation=0;
        ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
        CHECK(stamps==7 && !bad_rotation); /* seven non-space glyphs */
        CHECK(!strchr(ui_widget_text(m->ui,stamp),'\n'));
    }
    printf("PASS sold-out stamp: one line, authored angle, shared pivot; 120 draws %.3fs\n",
           (double)(clock()-start)/CLOCKS_PER_SEC);
    stamp->visible=0;
    start=clock();
    for (int i=0;i<120;i++) {
        stamps=0;
        ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
        CHECK(!stamps);
    }
    printf("Diagnostic without stamp: 120 draws %.3fs (stub renderer, not GPU timing)\n",
           (double)(clock()-start)/CLOCKS_PER_SEC);
    menu_close(m); gt4_save_shutdown();
    gt4_career_vehicle_close(factory); gt4_vol_close(vol);
    return 0;
}
