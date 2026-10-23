/* Native tuning callbacks and the actual equipped vehicle statistics. */
#define main setting_fixture_main
#include "../gtmode/setting_menu.c"
#undef main
int main(int argc, char **argv)
{
    CHECK(argc>=2 && gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    gt4_career_vehicle *factory = gt4_career_vehicle_open(argv[1]);
    CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats, factory);
    char msg[256];
    menu *m = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(m);
    ah_vm *vm = m->vm;
    ah_obj *car = ah_module_new(vm, "Car", NULL, NULL, NULL);
    CHECK(gt4_game_construct_car(vm,car,"ka_01",0,0));
    ah_val bound, arg = ah_objv(car);
    CHECK(nat_bind_game_car(vm,NULL,m,1,&arg,&bound)>=0);
    ah_obj *garage = get(vm, game_object(m), "garage").u.o, *cars = ah_array_new(vm);
    ah_array_push(vm, cars, arg);
    ah_set_member(vm, garage, "cars", ah_objv(cars));
    uint8_t stock[0x4b8], body[0x4b8];
    int32_t before[3], after[3];
    CHECK(gt4_game_car_body(vm,car,stock) && gt4_career_vehicle_stats(factory,stock,before));
    const gt4_career_part_data *data = gt4_game_part_data();
    CHECK(data);
    unsigned changed = 0;
    for (int type = 1; type <= 31; type++)
        for (int stage = 1; stage <= data->type_max[type - 1]; stage++) {
            ah_val params[] = {ah_int(type), ah_int(stage)}, key, ret;
            CHECK(ah_call_member(vm,car,"_getSettingPartKey",2,params,&key)==1);
            if ((uint64_t)key.u.i == UINT64_MAX)
                continue;
            unsigned table = (unsigned)((uint64_t)key.u.i >> 32);
            CHECK(table<39);
            const char *name = data->table_names[table];
            int weight = strstr(name, "LIGHTWEIGHT") != NULL, power = !strcmp(name, "MUFFLER");
            if (!weight && !power)
                continue;
            CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",stock,sizeof stock));
            CHECK(ah_call_member(vm,car,"setParts",2,params,&ret)==1);
            CHECK(gt4_game_car_body(vm,car,body) && memcmp(body,stock,sizeof body));
            CHECK(gt4_career_vehicle_stats(factory,body,after));
            fprintf(stderr, "%s stage%d power %d -> %d weight %d -> %d\n", name, stage, before[0],
                    after[0], before[2], after[2]);
            if (argc > 2)
                memcpy(after, before, sizeof after);
            if (weight) {
                CHECK(after[2]<before[2]);
                changed |= 1;
            }
            if (power) {
                CHECK(after[0]>before[0]);
                changed |= 2;
            }
        }
    CHECK(changed==3);
    menu_close(m);
    gt4_game_set_stats_reader(NULL, NULL);
    gt4_game_set_car_factory(NULL, NULL);
    gt4_career_vehicle_close(factory);
    gt4_vol_close(vol);
    puts("PASS equipped weight reduction and muffler upgrades change real vehicle statistics");
    return 0;
}
