/* Original award execution and cross-project persistence. Eligibility is
   a controlled fixture here, not a physical victory or a completion claim. */
#define GT4_SERIES_EMBEDDED 1
#include "series_playable.c"

static int awarded_flag(menu *m, int bit)
{
    ah_vm *vm = m->vm;
    ah_obj *present = get(vm, game_object(m), "present").u.o;
    ah_val id = ah_int(bit), value;
    CHECK(ah_call_member(vm, present, "getByRace", 1, &id, &value) >= 0);
    printf("award bit %d=%d\n", bit, ah_truthy(value));
    int actual = ah_truthy(value);
    if (getenv("GT4_PRESENT_CYCLE_NEGATIVE"))
        actual ^= 1;
    CHECK(actual);
    return 0;
}

static int controlled_award(menu *m, gt4_career_vehicle *factory, int id, series_prize prizes[3],
                            unsigned *count)
{
    CHECK(menu_enter_career_return(m));
    ticks(m);
    ah_vm *vm = m->vm;
    unsigned first = ah_array_size(get(vm, get(vm, game_object(m), "garage").u.o, "cars").u.o);
    ah_obj *page = get(vm, m->project, "PresentRoot").u.o;
    ah_obj *list = ah_array_new(vm);
    ah_array_push(vm, list, ah_int(id));
    ah_set_member(vm, page, "current_list", ah_objv(list));
    CHECK(enter_named(m, "PresentRoot"));
    ticks(m);
    CHECK(!collect_prizes(m, factory, prizes, 3, count, first));
    menu_save_capture(m);
    CHECK(gt4_save_store() && gt4_save_load_slot(1));
    menu_save_refresh(m);
    CHECK(!awarded_flag(m, id) && !reload_prizes(m, prizes, *count));
    /* Original: gtmode.ad achievement L14245..14250. Delivered awards
       remain ineligible even with the controlled threshold still met. */
    ah_val key = ah_int(id), column, entitled;
    CHECK(ah_call_member(vm, page, "makeColumn", 1, &key, &column) >= 0);
    ah_val check[] = {key, column};
    ah_set_member(vm, m->project, "cached_achievement", ah_int(1000));
    CHECK(ah_call_member(vm, page, "achievement", 2, check, &entitled) >= 0);
    CHECK(!ah_truthy(entitled));
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *error;
    char message[256], directory[200];
    snprintf(directory, sizeof directory, "build/present-cycle-%llu-%lu",
             (unsigned long long)time(NULL), test_process_id());
    gt4_vol *vol = gt4_vol_open(argv[1], &error);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory = gt4_career_vehicle_open(argv[1]);
    CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, factory);
    gt4_save_init(directory);
    menu *m = menu_open(vol, "gtmode", message, sizeof message);
    CHECK(m);
    menu_set_car_loader(m, transaction_model_loader, NULL);
    menu_save_capture(m);
    CHECK(gt4_save_create(1, "Present persistence"));
    series_prize prizes[3];
    unsigned count = 0;
    CHECK(!controlled_award(m, factory, 169, prizes, &count));
    CHECK(!controlled_award(m, factory, 170, prizes, &count));
    CHECK(!controlled_award(m, factory, 171, prizes, &count));
    CHECK(count == 3);
    menu *quick = menu_open(vol, "quick-championship", message, sizeof message);
    CHECK(quick);
    menu_save_refresh(quick);
    CHECK(!awarded_flag(quick, 169) && !awarded_flag(quick, 170) && !awarded_flag(quick, 171));
    menu_save_capture(quick);
    menu_save_refresh(m);
    CHECK(!reload_prizes(m, prizes, count));
    CHECK(!awarded_flag(m, 169) && !awarded_flag(m, 170) && !awarded_flag(m, 171));
    menu_close(quick);
    menu_close(m);
    gt4_save_shutdown();
    gt4_game_set_car_factory(NULL, NULL);
    gt4_career_vehicle_close(factory);
    gt4_vol_close(vol);
    puts(
        "PASS original 25/50/100-percent awards, bodies, flags, one-shot eligibility and championship VM");
    return 0;
}
