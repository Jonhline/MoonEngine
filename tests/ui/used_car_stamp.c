/* Exercise the original UsedCarRoot resource and its sold-out label. */
#define main purchase_fixture_main
#define plat_draw_rotated_uv fixture_rotated_uv
#define plat_draw_solid_quad fixture_solid_quad
#define plat_draw_quad fixture_quad
#include "../gtmode/purchase_menu.c"
#undef plat_draw_rotated_uv
#undef plat_draw_solid_quad
#undef plat_draw_quad
#undef main

static unsigned touch_chip_overlap;
static float protected_chip[4];
static void check_touch_color(float x, float y, float w, float h, const uint8_t c[4])
{
    if (c[0] == 255 && c[1] == 220 && c[2] == 128 && c[3] &&
        x < protected_chip[0] + protected_chip[2] && x + w > protected_chip[0] &&
        y < protected_chip[1] + protected_chip[3] && y + h > protected_chip[1])
        touch_chip_overlap++;
}
void plat_draw_solid_quad(const float xy[4][2], const uint8_t c[4])
{
    check_touch_color(xy[0][0], xy[0][1], xy[1][0] - xy[0][0], xy[2][1] - xy[1][1], c);
    fixture_solid_quad(xy, c);
}
void plat_draw_quad(const plat_texture *t, float x, float y, float w, float h, float u, float v,
                    float a, float b, const uint8_t c[4])
{
    check_touch_color(x, y, w, h, c);
    fixture_quad(t, x, y, w, h, u, v, a, b, c);
}

void plat_video_set_view(float w,float h) {(void)w;(void)h;}
static unsigned stamps;
static float pivot_x, pivot_y;
static int bad_rotation;
static unsigned license_stamps;
static float license_x,license_y;
static int license_bad;
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
    if(c[0]==221 && c[1]==67 && c[2]==35) {
        license_stamps++;
        if (angle != -20 || fabsf(cx - license_x) > .001f || fabsf(cy - license_y) > .001f ||
            y < license_y - 32)
            license_bad++;
    }
    plat_draw_glyph(t, x, y, w, h, y, 0, u0, v0, u1, v1, c);
}

static int ticket_hover(gt4_vol *vol)
{
    char msg[256];
    menu *m = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(m);
    menu_enter(m, 0);
    ticks(m);
    ah_obj *garage = get(m->vm, game_object(m), "garage").u.o;
    ah_val buy[] = {ah_str(m->vm, "ka_01"), ah_int(0)}, result, index = ah_int(0);
    CHECK(ah_call_member(m->vm, garage, "addNewCar", 2, buy, &result) == 1);
    CHECK(ah_call_member(m->vm, garage, "setRidingCar", 1, &index, &result) == 1);
    const menu_input keys[] = {MENU_UP,     MENU_UP,    MENU_ACCEPT, MENU_RIGHT, MENU_RIGHT,
                               MENU_ACCEPT, MENU_RIGHT, MENU_RIGHT,  MENU_RIGHT, MENU_ACCEPT};
    for (unsigned i = 0; i < sizeof keys / sizeof *keys; i++) {
        menu_send_input(m, keys[i]);
        ticks(m);
    }
    CHECK(!strcmp(ui_root_name(m->ui, menu_active_root(m)), "EventHallRoot"));
    ui_widget *root = ui_root_widget(m->ui, menu_active_root(m));
    ui_widget *list = kid(kid(root, "ScrollBox"), "SelectBox");
    CHECK(list && list->nkids >= 2);
    for (int variant = 0; variant < 2; variant++) {
        if (variant) {
            ui_widget *full = ui_widget_copy(m->ui, kid(kid(root, "hidden"), "EventItem"));
            CHECK(full);
            ui_widget_set_image(kid(kid(full, "Ticket"), "image"),
                                kid(kid(list->kids[0], "Ticket"), "image")->image_path);
            ui_widget_append(list, full);
            ui_widget *first = list->kids[0];
            list->kids[0] = full;
            list->kids[list->nkids - 1] = first;
        }
        ui_widget *card = list->kids[0], *ticket = kid(card, "Ticket");
        CHECK(ticket);
        ui_widget *label = kid(ticket, "label"), *neighbor = kid(list->kids[1], "Ticket");
        if (!label)
            label = kid(ticket, "level");
        CHECK(label && neighbor);
        ui_pointer(m->ui, 1, 200, 180);
        ui_pointer_feedback(m->ui, 0);
        ui_pointer_hover(m->ui, card);
        for (int i = 0; i < 60; i++)
            ui_animate(m->ui, menu_active_root(m), 1.f / 60);
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
        float width = ticket->pointer_rect[2], other = neighbor->pointer_rect[2];
        float text_width = label->pointer_rect[2], authored = ticket->w;
        ui_pointer_feedback(m->ui, 1);
        ui_animate(m->ui, menu_active_root(m), 1.f / 60);
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
        CHECK(ticket->pointer_rect[2] > width && ticket->pointer_rect[2] < width * 1.045f);
        for (int i = 0; i < 60; i++)
            ui_animate(m->ui, menu_active_root(m), 1.f / 60);
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
        CHECK(ticket->pointer_rect[2] > width * 1.04f);
        CHECK(label->pointer_rect[2] > text_width * 1.04f);
        CHECK(fabsf(neighbor->pointer_rect[2] - other) < .001f && ticket->w == authored);
        ui_pointer(m->ui, 0, 0, 0);
        ui_touch_mode(m->ui, 1);
        ui_touch_preview(m->ui, card);
        for (int i = 0; i < 60; i++)
            ui_animate(m->ui, menu_active_root(m), 1.f / 60);
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
        CHECK(ticket->pointer_rect[2] > width * 1.04f);
        ui_touch_mode(m->ui, 0);
        ui_pointer_feedback(m->ui, 0);
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
        CHECK(fabsf(ticket->pointer_rect[2] - width) < .001f);
    }
    menu_close(m);
    puts(
        "PASS event/championship tickets: eased whole-card mouse/touch feedback, Classic unchanged");
    return 0;
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
    /* Whole used-car row: name, specs, price and chip share the same zoom. */
    ui_widget *row=list->kids[1], *name=kid(row,"Name"), *price=kid(row,"Price"),
              *spec=kid(row,"Spec0"), *chip=kid(row,"chip");
    ui_touch_mode(m->ui,0);ui_pointer_feedback(m->ui,1);ui_pointer(m->ui,1,100,120);
    ui_pointer_hover(m->ui,NULL);
    ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
    float name_width=name->pointer_rect[2], price_height=price->pointer_rect[3],
          spec_width=spec->pointer_rect[2], chip_height=chip->pointer_rect[3];
    float original_width=name->w;
    ui_pointer_hover(m->ui,row);
    for(int tick=0;tick<60;tick++) ui_animate(m->ui,menu_active_root(m),1.f/60);
    if(getenv("MOON_CARD_HOVER_NEGATIVE")) ui_pointer_feedback(m->ui,0);
    ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
    CHECK(name->pointer_rect[2]>name_width*1.04f);
    CHECK(price->pointer_rect[3]>price_height*1.04f);
    CHECK(spec->pointer_rect[2]>spec_width*1.04f);
    CHECK(chip->pointer_rect[3]>chip_height*1.04f);
    CHECK(name->w==original_width);
    /* A gold touch accent must never tint the used-car paint sample. */
    ui_pointer(m->ui, 0, 0, 0);
    ui_touch_mode(m->ui, 1);
    ui_touch_preview(m->ui, row);
    for (int tick = 0; tick < 60; tick++) {
        ui_animate(m->ui, menu_active_root(m), 1.f / 60);
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
        memcpy(protected_chip, chip->pointer_rect, sizeof protected_chip);
        touch_chip_overlap = 0;
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
        if (getenv("MOON_TOUCH_CHIP_NEGATIVE")) {
            const uint8_t gold[4] = {255, 220, 128, 220};
            plat_draw_quad(NULL, protected_chip[0], protected_chip[1], 2, 2, 0, 0, 1, 1, gold);
        }
        CHECK(!touch_chip_overlap);
    }
    ui_touch_mode(m->ui, 0);
    ui_pointer_feedback(m->ui,0);
    ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
    CHECK(fabsf(name->pointer_rect[2]-name_width)<.001f);
    ui_pointer_hover(m->ui,NULL);
    CHECK(enter_named(m,"LicenseRoot"));ticks(m);
    root=ui_root_widget(m->ui,menu_active_root(m));
    const char *groups[]={"l0b","l0a","lib","lia","l0s"};
    for(int group=0;group<5;group++) {
        for(int j=0;j<5;j++) kid(kid(root,groups[j]),"Passed")->visible=j==group;
        ui_widget *entry=kid(root,groups[group]);
        ui_widget *passed=kid(entry,"Passed"), *label=kid(passed,"label");
        CHECK(passed->w==128 && passed->h==128 && !label->has_geometry);
        license_x=root->x+entry->x+passed->x+64;
        license_y=root->y+entry->y+passed->y+64;
        if(getenv("MOON_LICENSE_STAMP_NEGATIVE")) {
            label->has_runtime_align=1;label->runtime_align=1;
        }
        license_stamps=0;license_bad=0;
        ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);
        CHECK(license_stamps==6 && !license_bad);
    }
    printf("PASS five license stamps: centered client, inherited -20 rotation\n");
    CHECK(ticket_hover(vol) == 0);
    menu_close(m); gt4_save_shutdown();
    gt4_career_vehicle_close(factory); gt4_vol_close(vol);
    return 0;
}
