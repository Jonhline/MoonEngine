/* C: native pad-driven mission -> authored board -> prize -> save/reload.
   P test-driver inputs; no car state or finish/prize flags are injected. */
#define main pregrid_embedded_main
/* Reuse the headless platform, but not its Arcade-only icon dimensions. */
#define plat_draw_quad4 pregrid_arcade_quad4
#include "../test_process.h"
#include "../ui/pregrid_menu.c"
#undef plat_draw_quad4
#undef main
#include "../../src/core/race_result.c"
#include "career_license_disc.h"
#include "physics/car.h"
#include "race_mission.h"
#include "mission_pad_fixture.h"
#include <time.h>
void plat_video_set_view(float w, float h)
{
    (void)w;
    (void)h;
}
void plat_draw_quad4(const plat_texture *t, float x, float y, float w, float h, float u, float v,
                     float a, float b, const uint8_t c[4][4])
{
    (void)t;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)u;
    (void)v;
    (void)a;
    (void)b;
    (void)c;
    quads++;
}
static gt4_race_run *driven;
static uint32_t player;
static int drive(void *u, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    if (car != player)
        return 0;
    memset(cmd, 0, 32);
    cmd[3] = ev[3];
    if (u) {
        if (fread(cmd, 1, 32, (FILE *)u) != 32) {
            memset(cmd, 0, 32);
            cmd[1] = 1;
        }
        if (getenv("GT4_MISSION_COMMAND_NEGATIVE")) {
            memset(cmd, 0, 32);
            cmd[1] = 1;
        }
    } else
        mission_pad_fixture(driven, car, cmd);
    return 1;
}
static int wheel(void *u, uint32_t car, int16_t out[2])
{
    (void)u;
    (void)car;
    out[0] = out[1] = 0;
    return 1;
}
static int vibration(void *u, uint32_t car, const uint8_t *out)
{
    (void)u;
    (void)car;
    (void)out;
    return 1;
}
static unsigned descriptions;
static void check_text(ui_project *p, ui_widget *w)
{
    if (!strcmp(w->type, "TextFace") && mp_number(w->src, "multiline", 0) && w->w > 0) {
        float size[2];
        ui_text_size(p, w, size);
        const char *text = ui_widget_text(p, w);
        if (text && *text) {
            if (getenv("GT4_MISSION_TEXT_NEGATIVE"))
                size[0] = w->w + 1;
            if (size[0] > w->w + .001f)
                fprintf(stderr, "text %s width %g/%g: %s\n", w->name, size[0], w->w, text);
            CHECK(size[0]<=w->w+.001f);
            descriptions++;
        }
    }
    for (int i = 0; i < w->nkids; i++)
        check_text(p, w->kids[i]);
}
int main(int argc, char **argv)
{
    CHECK(argc == 2 || argc == 3);
    const char *winning_code = argc == 3 ? argv[2] : "l0m0009";
    const char *err;
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    char dir[200], msg[256];
    snprintf(dir, sizeof dir, "build/mission-cycle-%llu-%llu-%lu", (unsigned long long)time(NULL),
             (unsigned long long)clock(), test_process_id());
    gt4_save_init(dir);
    menu *career = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(career);
    menu_save_capture(career);
    CHECK(gt4_save_create(1,"Mission validation"));
    int hall = -1;
    for (int i = 0; i < ui_root_count(career->ui); i++)
        if (!strcmp(ui_root_name(career->ui, i), "MissionRoot"))
            hall = i;
    CHECK(hall>=0);
    menu_enter(career, hall);
    advance(career);
    ui_draw_root(career->ui, menu_active_root(career), NULL, NULL);
    check_text(career->ui, ui_root_widget(career->ui, menu_active_root(career)));
    CHECK(descriptions>=34);
    gt4_career_race_lookup_data lookup;
    CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
    gt4_career_license_exam mission;
    CHECK(gt4_career_mission_prepare(&mission,argv[1],&lookup,winning_code,msg,sizeof msg));
    driven = gt4_race_run_open(argv[1], &mission.desc, msg, sizeof msg);
    CHECK(driven);
    player = gt4_race_run_car(driven, 0);
    const char *commands_path = getenv("GT4_MISSION_COMMANDS");
    FILE *commands = commands_path ? fopen(commands_path, "rb") : NULL;
    CHECK(!commands_path || commands);
    gt4_phys_pad.ctx = commands;
    if (getenv("GT4_MISSION_TAKEOVER"))
        CHECK(gt4_car_take_over(player, 1));
    gt4_phys_pad.read = drive;
    gt4_phys_pad.wheel_state = wheel;
    gt4_phys_pad.write = vibration;
    int outcome = 0;
    unsigned frame;
    for (frame = 0; frame < 86400 && !outcome; frame++) {
        if (getenv("GT4_MISSION_TAKEOVER") && !GT4_U8(player + CAR_56B))
            CHECK(gt4_car_take_over(player, 1));
        CHECK(gt4_race_run_frame(driven));
        uint32_t race = GT4_U32(gt4_race_run_conductor(driven));
        outcome = gt4_mission_presentation(gt4_ptr(race + 0xf4), outcome);
    }
    gt4_race_hud hud;
    CHECK(gt4_race_run_hud(driven,0,&hud));
    CHECK(outcome==GT4_MISSION_BOARD && hud.finished && hud.rank==1);
    if (commands) {
        CHECK(!ferror(commands));
        fclose(commands);
        gt4_phys_pad.ctx = NULL;
    }
    uint32_t prize = gt4_career_race_prize(mission.row, 0), award = 0;
    CHECK(prize>0);
    CHECK(menu_apply_mission_result(career,winning_code,hud.finished,0,hud.best_lap,hud.aspec_points,prize,&award));
    CHECK(award==prize);
    menu_save_capture(career);
    menu_quick_race q = {0};
    strcpy(q.race, winning_code);
    strcpy(q.course, mission.desc.course);
    q.entries = mission.desc.cars;
    q.player = 0;
    q.finished = 1;
    q.prize = award;
    load_car_tables(career);
    for (int i = 0; i < q.entries; i++) {
        const char *label = gt4_specdb_label_for_id(career->car_labels, career->car_labels_size,
                                                    mission.desc.car[i]);
        gt4_race_hud entry;
        CHECK(label && gt4_race_run_hud(driven, i, &entry));
        snprintf(q.grid[i].car, sizeof q.grid[i].car, "%s", label);
        q.grid[i].time = entry.line_time;
    }
    menu_set_quick_race(&q);
    menu *quick = menu_open(vol, "quick-mission", msg, sizeof msg);
    CHECK(quick);
    int64_t before = 0;
    ah_val money;
    CHECK(ah_get_member(career->vm,game_object(career),"money",&money));
    before = money.u.i;
    CHECK(menu_enter_quick_start(quick));
    advance(quick);
    CHECK(!strcmp(ui_root_name(quick->ui,menu_active_root(quick)),"PrizeRoot"));
    ah_val paid;
    CHECK(ah_call_member(quick->vm, quick->project, "makePrizeFast", 0, NULL, &paid) >= 0);
    for (unsigned i = 0; i < 1000; i++) {
        ah_val remaining;
        CHECK(ah_get_member(quick->vm, quick->project, "rest_prize", &remaining));
        if (ah_num(remaining) <= 0)
            break;
        advance(quick); /* authored prize countdown/watchers */
    }
    CHECK(ah_get_member(quick->vm,game_object(quick),"money",&money));
    CHECK(money.u.i==before+prize);
    menu_send_input(quick, MENU_ACCEPT);
    advance(quick);
    CHECK(!strcmp(ui_root_name(quick->ui,menu_active_root(quick)),"AfterRoot"));
    menu_save_capture(quick);
    CHECK(gt4_save_store() && gt4_save_load_slot(1));
    menu_save_refresh(career);
    ah_obj *records = NULL;
    ah_val val;
    CHECK(ah_get_member(career->vm,game_object(career),"race_record",&val));
    records = val.u.o;
    ah_val code = ah_str(career->vm, winning_code), unit;
    CHECK(ah_call_member(career->vm,records,"getUnit",1,&code,&unit)>=0);
    ah_val word, grade;
    CHECK(ah_get_member(career->vm,unit.u.o,"original_record",&word));
    grade = ah_int(((uint32_t)word.u.i >> 4) & 15);
    if (getenv("GT4_MISSION_CYCLE_NEGATIVE"))
        grade = ah_int(1);
    CHECK(ah_num(grade)==0);
    CHECK(ah_get_member(career->vm,game_object(career),"money",&money));
    CHECK(money.u.i==before+prize);
    q.finished = 0;
    q.prize = 0;
    menu_set_quick_race(&q);
    for (unsigned i = 1; i <= 34; i++) {
        char mission_code[32];
        snprintf(mission_code, sizeof mission_code, "l0m%04u", i);
        menu_quick_event_context(quick, mission_code);
        CHECK(menu_enter_quick_start(quick));
        advance(quick);
        ui_draw_root(quick->ui, menu_active_root(quick), NULL, NULL);
        ui_widget *root = ui_root_widget(quick->ui, menu_active_root(quick));
        check_text(quick->ui, root);
    }
    race_result *board = race_result_mission_open(vol, NULL, NULL);
    CHECK(board && board->exit_ok && board->mission);
    race_result_state s = {0};
    s.cars = q.entries;
    s.player = 0;
    for (int i = 0; i < s.cars; i++) {
        gt4_race_hud entry;
        CHECK(gt4_race_run_hud(driven, i, &entry));
        s.rank[i] = entry.rank;
        s.finished[i] = entry.finished;
        s.time[i] = entry.line_time;
        s.name[i] = q.grid[i].car;
    }
    for (int i = 0; i < 480 && !race_result_done(board); i++) {
        race_result_update(board, &s, .001);
        if (i % 20 == 0)
            race_result_press(board);
    }
    CHECK(race_result_done(board));
    race_result_close(board);
    CHECK(!errors);
    gt4_race_run_close(driven);
    driven = NULL;
    if (argc == 2) {
        /* A losing physical attempt must leave its board closed and grant no
    prize. Mission 10's fixture reaches the goal behind all three rivals. */
        CHECK(gt4_career_mission_prepare(&mission,argv[1],&lookup,"l0m0010",msg,sizeof msg));
        driven = gt4_race_run_open(argv[1], &mission.desc, msg, sizeof msg);
        CHECK(driven);
        player = gt4_race_run_car(driven, 0);
        outcome = 0;
        for (unsigned i = 0; i < 14400 && !outcome; i++) {
            CHECK(gt4_race_run_frame(driven));
            uint32_t race = GT4_U32(gt4_race_run_conductor(driven));
            outcome = gt4_mission_presentation(gt4_ptr(race + 0xf4), outcome);
        }
        CHECK(outcome==GT4_MISSION_RETURN);
        CHECK(gt4_race_run_hud(driven,0,&hud) && hud.rank==4);
        uint32_t losing_prize = gt4_career_race_prize(mission.row, 3);
        CHECK(!losing_prize);
        award = 0;
        CHECK(menu_apply_mission_result(career,"l0m0010",hud.finished,3,hud.best_lap,hud.aspec_points,losing_prize,&award) && !award);
        CHECK(ah_get_member(career->vm,game_object(career),"money",&money) && money.u.i==before+prize);
        menu_save_capture(career);
        menu_save_refresh(quick);
        q.finished = 1;
        q.prize = 0;
        strcpy(q.race, "l0m0010");
        menu_set_quick_race(&q);
        CHECK(menu_enter_quick_start(quick));
        advance(quick);
        CHECK(!strcmp(ui_root_name(quick->ui,menu_active_root(quick)),"AfterRoot"));
    }
    printf(
        "PASS mission %s: %u native frames, rank 1, prize %u, authored PrizeRoot/AfterRoot, persisted record/credits; %u multiline descriptions.\n",
        winning_code, frame, prize, descriptions);
    if (argc == 2)
        puts(
            "PASS mission 10: native losing attempt returns without result board or credits, authored AfterRoot.");
    if (driven)
        gt4_race_run_close(driven);
    menu_close(quick);
    menu_close(career);
    gt4_save_shutdown();
    gt4_vol_close(vol);
    return 0;
}
