#include <SDL.h>
#define main fitted_parts_original_main
#include "fitted_parts.c"
#undef main
#include "career_event.h"
static void verify_career(race_game *g, menu *career, const gt4_career_event *event, int negative)
{
    char msg[256];
    uint8_t before[0x180], oldskill[8], after[0x180], skill[8], expected[0x180], wanted[8],
        style[0x40];
    CHECK(menu_bspec_profile(career,event->desc.course,before,oldskill,0));
    CHECK(race_game_start_prepared(g,&event->desc,msg,sizeof msg));
    ready(g);
    uint32_t car = gt4_race_run_car(g->run, 0), cond = gt4_race_run_conductor(g->run);
    gt4_career_bspec_seed(before, oldskill, g->bspec_band, style);
    CHECK(!memcmp(style,gt4_ptr(car+CAR_FIELD_DRIVING_STYLE),sizeof style));
    CHECK(race_game_restart(g,msg,sizeof msg));
    ready(g);
    car = gt4_race_run_car(g->run, 0);
    cond = gt4_race_run_conductor(g->run);
    CHECK(!memcmp(style,gt4_ptr(car+CAR_FIELD_DRIVING_STYLE),sizeof style));
    gt4_race_hud hud = {0};
    int ticks = 0;
    while (!hud.finished && ticks < 120000) {
        race_game_update(g, STEP + 1e-12);
        ticks++;
        CHECK(race_game_player_hud(g,&hud));
    }
    CHECK(hud.finished);
    fprintf(stderr, "B-Spec physical finish after %d ticks\n", ticks);
    memcpy(style, gt4_ptr(car + CAR_FIELD_DRIVING_STYLE), sizeof style);
    CHECK(memcmp(style,(uint8_t[0x40]){0},sizeof style));
    memcpy(expected, before, sizeof before);
    memcpy(wanted, oldskill, 8);
    int kind = GT4_U8(cond + CONDUCTOR_FIELD_BSPEC_COURSE_KIND) ? 1 : -1;
    int bonus = GT4_U8(car + CAR_5B6) && (int8_t)GT4_U8(car + CAR_FIELD_POSITION) == 1;
    gt4_career_bspec_commit(expected, wanted, g->bspec_band, kind, bonus, style);
    CHECK(race_game_commit_bspec(g,career));
    CHECK(menu_bspec_profile(career,event->desc.course,after,skill,0));
    if (negative)
        after[0] ^= 1;
    CHECK(!memcmp(expected,after,sizeof after) && !memcmp(wanted,skill,8));
    CHECK(memcmp(before,after,sizeof after));
    CHECK(race_game_commit_bspec(g,career));
    CHECK(menu_bspec_profile(career,event->desc.course,after,skill,0));
    CHECK(!memcmp(expected,after,sizeof after) && !memcmp(wanted,skill,8));
    CHECK(gt4_save_store());
    uint8_t zeros[0x180] = {0}, empty[8] = {0};
    CHECK(menu_bspec_profile(career,event->desc.course,zeros,empty,1));
    CHECK(gt4_save_load_slot(7));
    menu_save_refresh(career);
    CHECK(menu_bspec_profile(career,event->desc.course,after,skill,0));
    CHECK(!memcmp(expected,after,sizeof after) && !memcmp(wanted,skill,8));
    CHECK(race_game_restart(g,msg,sizeof msg));
    ready(g);
    car = gt4_race_run_car(g->run, 0);
    gt4_career_bspec_seed(after, skill, g->bspec_band, style);
    CHECK(!memcmp(style,gt4_ptr(car+CAR_FIELD_DRIVING_STYLE),sizeof style));
    g->replaying = 1;
    CHECK(!race_game_commit_bspec(g,career));
    g->replaying = 0;
    CHECK(menu_bspec_profile(career,event->desc.course,after,skill,0));
    CHECK(!memcmp(expected,after,sizeof after));
}
int main(int argc, char **argv)
{
    CHECK(argc>=3);
    int negative = argc > 3;
    const char *why;
    char msg[256];
    CHECK(!plat_video_init(&(plat_video_desc){640,448,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    char isolated[1200];
    snprintf(isolated, sizeof isolated, "%s/run-%llu", argv[2], (unsigned long long)plat_time_ns());
    CHECK(gt4_save_mkdir(argv[2]) && gt4_save_mkdir(isolated));
    menu_options_storage(NULL);
    gt4_save_init(isolated);
    CHECK(gt4_save_create(7,"BSpec test"));
    gt4_vol *vol = gt4_vol_open(argv[1], &why);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory = gt4_career_vehicle_open(argv[1]);
    CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, factory);
    menu *career = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(career);
    ah_obj *garage = field(career->vm, game_object(career), "garage").u.o;
    ah_val args[] = {ah_str(career->vm, "ka_01"), ah_int(0)}, ret, index = ah_int(0);
    CHECK(ah_call_member(career->vm,garage,"addNewCar",2,args,&ret)==1);
    CHECK(ah_call_member(career->vm,garage,"setRidingCar",1,&index,&ret)==1);
    uint8_t body[0x4b8];
    CHECK(gt4_game_car_body(career->vm,field(career->vm,garage,"riding_car").u.o,body));
    menu_save_capture(career);
    uint32_t rng;
    CHECK(gt4_career_event_rng_load(argv[1],&rng));
    gt4_career_event event;
    CHECK(gt4_career_event_prepare(&event,argv[1],gt4_game_race_lookup_data(),
        "am_sunday_0001",body,0,&rng,msg,sizeof msg));
    event.desc.bspec = 1;
    race_game *g = race_game_open(argv[1]);
    CHECK(g);
    race_game_set_career(g, career);
    verify_career(g, career, &event, negative);
    race_game_close(g);
    menu_close(career);
    gt4_game_set_car_factory(NULL, NULL);
    gt4_career_vehicle_close(factory);
    gt4_vol_close(vol);
    gt4_save_shutdown();
    plat_video_shutdown();
    puts("PASS B-Spec career: live statistics, Retry seed, save/load and replay exclusion");
    return 0;
}
