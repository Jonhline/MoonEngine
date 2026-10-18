/* Save/Adhoc integration with injected attempt results. The native physical
   judgement has its separate EE oracle; this is C for the profile bridge. */
#define main pregrid_fixture_main
#include "../test_process.h"
#define plat_draw_quad4 fixture_draw_quad4
#include "../ui/pregrid_menu.c"
#undef plat_draw_quad4
#undef main
#include "career_license_disc.h"
static int capture_cones, cone_quads, oversized_cones;
void plat_draw_quad4(const plat_texture *t, float x, float y, float w, float h, float u, float v,
                     float a, float b, const uint8_t c[4][4])
{
    if (capture_cones && t) {
        if (w == 66 && h == 82.5f)
            cone_quads++;
        if (w == 88 && h == 110)
            oversized_cones++;
    }
    fixture_draw_quad4(t, x, y, w, h, u, v, a, b, c);
}

static ah_val field(ah_vm *vm, ah_obj *obj, const char *name)
{
    ah_val value;
    CHECK(ah_get_member(vm,obj,name,&value));
    return value;
}
static unsigned check_admission(menu *quick, unsigned completed)
{
    menu_save_refresh(quick);
    load_race_tables(quick);
    uint32_t count;
    memcpy(&count, quick->race_db + 8, sizeof count);
    unsigned checked = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t row[151];
        uint32_t id;
        CHECK(gt4_specdb_row_at(quick->race_db, quick->race_db_size, i, row, sizeof row) ==
              sizeof row);
        memcpy(&id, quick->race_db + 16 + i * 8, sizeof id);
        const char *code = gt4_specdb_label_for_id(quick->race_labels, quick->race_labels_size, id);
        CHECK(code);
        unsigned requirement = (unsigned)gt4_career_race_field(row, GT4_RACE_LICENSE);
        CHECK(menu_career_aspec_available(quick, code) ==
              (!requirement || (requirement <= 5 && requirement <= completed)));
        checked++;
    }
    return checked;
}

int main(int argc, char **argv)
{
    CHECK(argc==2);
    gt4_vol *v = gt4_vol_open(argv[1], NULL);
    CHECK(v && gt4_game_load_disc(argv[1]));
    char directory[200], error[256];
    snprintf(directory, sizeof directory, "build/career-native/license-cycle-%llu-%llu-%lu",
             (unsigned long long)time(NULL), (unsigned long long)clock(), test_process_id());
    gt4_save_init(directory);
    menu *m = menu_open(v, "gtmode", error, sizeof error);
    CHECK(m);
    ah_set_log(m->vm, log_script, NULL);
    menu_save_capture(m);
    CHECK(gt4_save_create(1,"Licence validation"));
    menu *q = menu_open(v, "quick-license", error, sizeof error);
    CHECK(q);
    menu *championship = menu_open(v, "quick-championship", error, sizeof error);
    CHECK(championship);
    menu *event = menu_open(v, "quick-event", error, sizeof error);
    CHECK(event);
    unsigned admission_checks = 0;
    ah_set_log(q->vm, log_script, NULL);
    const char *groups[] = {"l0b", "l0a", "lib", "lia", "l0s"};
    unsigned attempts = 0;
    for (unsigned g = 0; g < 5; g++) {
        ah_val category = ah_str(m->vm, groups[g]), passed;
        CHECK(ah_call_member(m->vm,field(m->vm,game_object(m),"race_record").u.o,"isLicensePassed",1,&category,&passed)>=0 && !ah_truthy(passed));
        for (unsigned n = 1; n <= 16; n++) {
            char code[32];
            snprintf(code, sizeof code, "%s%04u", groups[g], n);
            gt4_career_license_attempts visit;
            if (n == 15) {
                menu_save_refresh(q);
                ah_val gate, arg = ah_str(q->vm, code);
                ah_obj *root = field(q->vm, q->project, "LicenseRoot").u.o;
                CHECK(ah_call_member(q->vm,root,"available_next",1,&arg,&gate)>=0 && !ah_truthy(gate));
            }
            char next[32];
            CHECK(gt4_career_license_index(code)==(int)(g*16+n-1));
            CHECK(gt4_career_license_next(code,next)==(n<16));
            if (n < 16) {
                char expected[32];
                snprintf(expected, sizeof expected, "%s%04u", groups[g], n + 1);
                CHECK(!strcmp(next,expected));
            }
            /* No garage car: examination owns its car. */
            ah_set_member(m->vm, game_object(m), "race_code", ah_str(m->vm, code));
            ah_set_member(m->vm, game_object(m), "next_sequence", ah_str(m->vm, "license"));
            char request[32];
            CHECK(menu_get_license_request(m,request) && !strcmp(request,code));
            CHECK(menu_license_begin(m,code,&visit));
            CHECK(menu_license_attempt(m,code,&visit,-1,0x157529ffu,0));
            CHECK(visit.best_grade==13);
            CHECK(menu_license_attempt(m,code,&visit,4,60000,0x060a07eau));
            CHECK(menu_license_attempt(m,code,&visit,3,59000,0x060a07eau));
            CHECK(menu_license_attempt(m,code,&visit,2,58000,0x060a07eau));
            CHECK(menu_license_attempt(m,code,&visit,4,61000,0x060a07eau));
            attempts += 5;
            CHECK(visit.best_grade==9 && visit.best_time==58000);
            if (getenv("GT4_LICENSE_CYCLE_NEGATIVE") && !g && n == 1)
                visit.best_time++;
            CHECK(visit.best_time==58000 && menu_license_best(m,code)==58000);
            CHECK(menu_license_leave(m,code,&visit));
            CHECK(gt4_save_store() && gt4_save_load_slot(1));
            menu_save_refresh(m);
            menu_save_refresh(q);
            if (g == 1) {
                menu_save_refresh(championship);
                CHECK(menu_career_aspec_available(championship, "jprjapan_0500") == (n == 16));
            }
            if (n == 15) {
                ah_val gate, arg = ah_str(q->vm, code);
                ah_obj *root = field(q->vm, q->project, "LicenseRoot").u.o;
                CHECK(ah_call_member(q->vm,root,"available_next",1,&arg,&gate)>=0);
                if (getenv("GT4_LICENSE_GATE_NEGATIVE"))
                    gate = ah_int(0);
                CHECK(ah_truthy(gate));
            }
            ah_val record, value, arg = ah_str(q->vm, code);
            CHECK(ah_call_member(q->vm,field(q->vm,game_object(q),"license_record").u.o,"getUnit",1,&arg,&record)>=0);
            CHECK(menu_license_best(q,code)==58000);
            CHECK(ah_call_member(q->vm,record.u.o,"getRankNum",0,NULL,&value)>=0 && ah_num(value)==4);
            CHECK(ah_call_member(q->vm,record.u.o,"getBestTime",0,NULL,&value)>=0 && !strcmp(ah_cstr(value),"0'58.000"));
            CHECK(ah_call_member(q->vm,record.u.o,"getResult",0,NULL,&value)>=0 && !strcmp(ah_cstr(value),"gold"));
            if (n == 1) {
                gt4_career_license_exam exam;
                CHECK(gt4_career_license_prepare(&exam,argv[1],gt4_game_race_lookup_data(),code,error,sizeof error));
                menu_quick_race work = {0};
                work.license = 1;
                work.entries = 1;
                work.player = 0;
                snprintf(work.course, sizeof work.course, "%s", exam.desc.course);
                snprintf(work.race, sizeof work.race, "%s", code);
                work.gold_time = gt4_career_license_time(exam.row, 0);
                work.silver_time = gt4_career_license_time(exam.row, 1);
                work.bronze_time = gt4_career_license_time(exam.row, 2);
                menu_set_quick_race(&work);
                menu_set_quick_best(visit.best_time);
                menu_quick_event_context(q, code);
                CHECK(menu_enter_quick_start(q));
                advance(q);
                CHECK(!strcmp(ui_root_name(q->ui,menu_active_root(q)),"LicenseRoot"));
                const char *names[] = {"goldTime", "silverTime", "bronzeTime"};
                uint32_t times[] = {work.gold_time, work.silver_time, work.bronze_time};
                for (int i = 0; i < 3; i++) {
                    char expected[32];
                    snprintf(expected, sizeof expected, "%u'%02u.%03u", times[i] / 60000,
                             times[i] / 1000 % 60, times[i] % 1000);
                    ah_val value = field(q->vm, q->quick_work, names[i]);
                    if (getenv("GT4_LICENSE_TIME_BRIDGE_NEGATIVE"))
                        value = ah_int(times[i]);
                    CHECK(ah_cstr(value) && !strcmp(ah_cstr(value),expected));
                }
                CHECK(!errors);
                /* Simulate the authored outgoing veil and cached page lifecycle.
       L8227 must execute again on return and refresh the medal widgets. */
                int detail = -1;
                for (int i = 0; i < ui_root_count(m->ui); i++)
                    if (!strcmp(ui_root_name(m->ui, i), "LicenseDetailRoot"))
                        detail = i;
                CHECK(detail>=0);
                menu_enter(m, detail);
                advance(m);
                ui_widget *veil = child(child(ui_root_widget(m->ui, detail), "Pane"), "Veil");
                veil->visible = 1;
                if (!getenv("GT4_LICENSE_RETURN_NEGATIVE"))
                    menu_license_return_prepare(m);
                menu_enter(m, detail);
                advance(m);
                CHECK(!veil->visible && !errors);
            }
        }
        category = ah_str(m->vm, groups[g]);
        CHECK(ah_call_member(m->vm,field(m->vm,game_object(m),"race_record").u.o,"isLicensePerfect",1,&category,&passed)>=0 && ah_truthy(passed));
        admission_checks += check_admission(championship, g + 1);
        admission_checks += check_admission(event, g + 1);
    }
    CHECK(ah_num(field(m->vm,field(m->vm,game_object(m),"calendar").u.o,"elapsed_days"))==80);
    CHECK(!errors);
    printf(
        "PASS %u injected attempts, 80 saved medal/ranking/calendar visits, five authored licence pre-race menus.\n",
        attempts);
    printf("PASS %u event/championship admission checks across all five licence categories.\n",
           admission_checks);
    for (unsigned g = 0; g < 5; g++) {
        ah_set_member(m->vm, m->project, "current_category", ah_str(m->vm, groups[g]));
        menu_license_return_prepare(m);
        for (int i = 0; i < ui_root_count(m->ui); i++) {
            if (strcmp(ui_root_name(m->ui, i), "LicenseDetailRoot"))
                continue;
            menu_enter(m, i);
            advance(m);
            cone_quads = oversized_cones = 0;
            capture_cones = 1;
            ui_draw_root(m->ui, i, NULL, NULL);
            capture_cones = 0;
            CHECK(cone_quads == 16 && oversized_cones == 0);
        }
    }
    puts("PASS 80 licence cones use the authored 66 x 82.5 client, not 88 x 110 texture size.");
    menu_close(event);
    menu_close(championship);
    menu_close(q);
    menu_close(m);
    gt4_save_shutdown();
    gt4_vol_close(v);
    return 0;
}
