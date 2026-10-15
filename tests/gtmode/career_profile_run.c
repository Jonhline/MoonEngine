/* Continuous-profile checkpoint. Only original menu APIs are queried;
   the test never assigns records, credits, dates or course availability. */
#define GT4_SERIES_EMBEDDED 1
#include "series_playable.c"

static int timed_courses(menu *m, int64_t days)
{
    ah_vm *vm = m->vm;
    ah_obj *data = get(vm, m->project, "PresentTimeData").u.o;
    ah_obj *types = get(vm, data, "type_carlist").u.o;
    ah_obj *contents = get(vm, data, "content_carlist").u.o;
    ah_obj *conditions = get(vm, data, "condition_carlist").u.o;
    ah_obj *details = get(vm, data, "detail_carlist").u.o;
    CHECK(types && contents && conditions);
    for (int i = 0; i < ah_array_size(types); i++) {
        if (strcmp(ah_cstr(ah_array_get(types, i)), "course"))
            continue;
        ah_val code = ah_array_get(contents, i), available;
        ah_val condition = ah_array_get(conditions, i);
        CHECK(ah_call_member(vm, game_object(m), "isCourseAvailable", 1, &code, &available) >= 0);
        /* Original: gtmode.ad PresentTimeData/PresentTimeRoot; weeks are
           read through MCalendar.getElapsedWeek (func_001331A0 +0x6C). */
        CHECK(!strcmp(ah_cstr(condition), "weeks"));
        const char *detail = ah_cstr(ah_array_get(details, i));
        CHECK(detail && *detail);
        int threshold = atoi(detail);
        ah_val elapsed;
        ah_obj *calendar = get(vm, game_object(m), "calendar").u.o;
        CHECK(ah_call_member(vm, calendar, "getElapsedWeek", 0, NULL, &elapsed) >= 0);
        int actual = ah_truthy(available), expected = ah_num(elapsed) >= threshold;
        if (getenv("GT4_CAREER_COURSE_NEGATIVE"))
            actual ^= 1;
        printf("timed course %s week=%g threshold=%d days=%lld available=%d\n", ah_cstr(code),
               ah_num(elapsed), threshold, (long long)days, actual);
        CHECK(actual == expected);
        ah_obj *courses = get(vm, m->project, "CourseData").u.o;
        ah_obj *course = get(vm, courses, ah_cstr(code)).u.o;
        if (ah_truthy(get(vm, course, "has_reverse"))) {
            char reverse[80];
            snprintf(reverse, sizeof reverse, "r_%s", ah_cstr(code));
            ah_val label = ah_str(vm, reverse), result;
            CHECK(ah_call_member(vm, game_object(m), "isCourseAvailable", 1, &label, &result) >= 0);
            int reverse_actual = ah_truthy(result);
            if (getenv("GT4_CAREER_REVERSE_NEGATIVE"))
                reverse_actual ^= 1;
            CHECK(reverse_actual == expected);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 6);
    const char *error;
    char message[256];
    gt4_vol *vol = gt4_vol_open(argv[1], &error);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_save_init(argv[2]);
    CHECK(gt4_save_load_slot(1));
    menu *m = menu_open(vol, "gtmode", message, sizeof message);
    CHECK(m);
    menu_save_refresh(m);
    menu_set_event_presenter(m, buy_event, NULL);
    /* Original: gtmode.ad TopRoot.onInitialize L1152 -> checkPresent
       L354..385. Load alone does not dismiss the previous prize screen. */
    CHECK(menu_enter_career_return(m));
    ticks(m);
    ah_vm *vm = m->vm;
    ah_obj *game = game_object(m);
    ah_val races;
    CHECK(ah_call_member(vm, game, "getTotalRaceCount", 0, NULL, &races) >= 0);
    int64_t days = get(vm, get(vm, game, "calendar").u.o, "elapsed_days").u.i;
    int64_t prizes = ah_num(get(vm, get(vm, game, "statistics").u.o, "prize_cars"));
    int64_t money = get(vm, game, "money").u.i;
    CHECK(atoll(argv[3]) < 0 || races.u.i == atoll(argv[3]));
    CHECK(atoll(argv[4]) < 0 || prizes == atoll(argv[4]));
    CHECK(atoll(argv[5]) < 0 || money == atoll(argv[5]));
    CHECK(!timed_courses(m, days));
    menu_save_capture(m);
    CHECK(gt4_save_store() && gt4_save_load_slot(1));
    menu_save_refresh(m);
    CHECK(!timed_courses(m, days));
    printf("PASS profile races=%lld prizes=%lld money=%lld days=%lld\n", (long long)races.u.i,
           (long long)prizes, (long long)money, (long long)days);
    menu_close(m);
    gt4_save_shutdown();
    gt4_vol_close(vol);
    return 0;
}
