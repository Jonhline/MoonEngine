/* Original static metadata and retail boot queries through their script bindings. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
static ah_obj *member(ah_vm *vm, ah_obj *o, const char *name)
{
    ah_val v;
    CHECK(ah_get_member(vm,o,name,&v));
    ah_obj *r = ah_as_obj(v, AO_MODULE);
    CHECK(r);
    return r;
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    char error[256];
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    menu *m = menu_open(vol, "quick", error, sizeof error);
    CHECK(m);
    ah_obj *main_ = member(m->vm, ah_root(m->vm), "main"), *menu_ = member(m->vm, main_, "menu");
    ah_val ret;
    CHECK(ah_call_member(m->vm,member(m->vm,menu_,"MSystem"),"IsOEM",0,NULL,&ret)>=0);
    CHECK(ah_num(ret)==0);
    CHECK(ah_call_member(m->vm,member(m->vm,menu_,"MUnit"),"GetUnitOfMass",0,NULL,&ret)>=0);
    CHECK(ah_cstr(ret) && !strcmp(ah_cstr(ret),"kg"));
    load_car_tables(m);
    CHECK(m->car_names && m->car_strings);
    ah_obj *cars = member(m->vm, menu_, "MCarData");
    int count = 0, different = 0;
    for (unsigned id = 0; id < 1000; id++) {
        const char *code = gt4_specdb_label_for_id(m->car_labels, m->car_labels_size, id);
        uint8_t row[16];
        if (!code || gt4_specdb_row(m->car_names, m->car_names_size, id, row, sizeof row) != 16)
            continue;
        const char *expected =
            gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(row + 8));
        const char *full = gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(row));
        CHECK(expected && full);
        ah_val argument = ah_str(m->vm, code);
        CHECK(ah_call_member(m->vm,cars,"GetShortCarName",1,&argument,&ret)>=0);
        if (strcmp(expected, full)) {
            different++;
            if (argc > 2)
                expected = full;
        }
        CHECK(ah_cstr(ret) && !strcmp(ah_cstr(ret),expected));
        count++;
    }
    CHECK(count>500 && different>50);
    ah_val bad = ah_str(m->vm, "not-a-car");
    CHECK(ah_call_member(m->vm,cars,"GetShortCarName",1,&bad,&ret)>=0);
    CHECK(ah_cstr(ret) && !*ah_cstr(ret));
    menu_close(m);
    gt4_vol_close(vol);
    printf("PASS retail OEM, original kg label and %d short car names\n", count);
    return 0;
}
