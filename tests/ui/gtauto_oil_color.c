/* OilChangeRoot authored colour endpoints, gtmode.ad L12018..19/12094. */
#define main purchase_fixture_main
#include "../gtmode/purchase_menu.c"
#undef main
int main(int argc, char **argv)
{
    CHECK(argc>=2 && gt4_game_load_disc(argv[1]));
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    gt4_save_init("");
    gt4_career_vehicle *factory = gt4_career_vehicle_open(argv[1]);
    CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, factory);
    gt4_game_set_service_reader(gt4_career_vehicle_service_geometry, factory);
    char msg[256];
    menu *m = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(m);
    ah_set_log(m->vm, log_script, NULL);
    menu_set_car_loader(m, transaction_model_loader, NULL);
    ah_obj *game = game_object(m), *garage = get(m->vm, game, "garage").u.o;
    ah_val ret;
    ah_val args[] = {ah_str(m->vm, "ka_01"), ah_int(0)};
    CHECK(ah_call_member(m->vm,garage,"addNewCar",2,args,&ret)==1);
    ah_val index = ah_int(0);
    CHECK(ah_call_member(m->vm,garage,"setRidingCar",1,&index,&ret)==1);
    ah_obj *car = get(m->vm, garage, "riding_car").u.o;
    uint8_t body[0x4b8];
    CHECK(gt4_game_car_body(m->vm,car,body));
    const uint8_t expected[2][3] = {{240, 195, 85}, {100, 90, 30}};
    for (int worn = 0; worn < 2; worn++) {
        garage = get(m->vm, game, "garage").u.o;
        car = get(m->vm, garage, "riding_car").u.o;
        float distance = worn ? 1e9f : 0;
        memcpy(body + 0x4a0, &distance, 4);
        body[0x4ac] = 0;
        gt4_career_blob_set(m->vm, car, "full_car_ps2", body, sizeof body);
        ah_obj *state = ah_array_new(m->vm);
        for (int i = 0; i < 32; i++)
            ah_array_push(m->vm, state, ah_int(body[0x498 + i]));
        ah_set_member(m->vm, car, "oil_state", ah_objv(state));
        CHECK(ah_call_member(m->vm,car,"getOilLifeRate",0,NULL,&ret)==1);
        CHECK(ah_num(ret)==worn);
        CHECK(gt4_save_capture(m->vm,game,1));
        CHECK(enter_named(m,"OilChangeRoot"));
        ui_widget *page = ui_root_widget(m->ui, menu_active_root(m));
        ui_widget *oil = kid(kid(page, "Gauge"), "oil");
        CHECK(oil && oil->has_runtime_color);
        for (int k = 0; k < 3; k++) {
            int value = oil->runtime_color[k];
            if (argc > 2)
                value = expected[1 - worn][k];
            CHECK(value==expected[worn][k]);
        }
        /* Reinitialize the same isolated page; no service or navigation needed. */
        m->entered[menu_active_root(m)] = 0;
    }
    CHECK(!script_errors);
    menu_close(m);
    gt4_game_set_service_reader(NULL, NULL);
    gt4_game_set_car_factory(NULL, NULL);
    gt4_career_vehicle_close(factory);
    gt4_vol_close(vol);
    puts("PASS original fresh gold and worn dark oil through authored page and VM");
    return 0;
}
