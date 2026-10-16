/* PC presentation contract; no game assets or PS2 camera equivalence. */
#include "launcher_scene.h"
#include "save.h"
#include "career_blob.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "scene line %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    char fixture[1600];
    snprintf(fixture, sizeof fixture, "%s/run-%llu", argv[1],
             (unsigned long long)SDL_GetPerformanceCounter());
    float first[9], other[9], fov, first_fov;
    launcher_scene_camera(0, 0, first, &first_fov);
    CHECK(first[0]==(argc>2 && !strcmp(argv[2],"--negative")?-4.7f:-4.8f) && first_fov==32);
    launcher_scene_camera(216, 0, other, &fov);
    CHECK(!memcmp(first,other,sizeof first) && fov==first_fov);
    launcher_scene_camera(3, 0, other, &fov);
    CHECK(memcmp(first,other,sizeof first));
    launcher_scene_camera(19, 1, other, &fov);
    CHECK(!memcmp(first,other,sizeof first));
    for (int i = 0; i < 2160; i++) {
        launcher_scene_camera(i * .1f, 0, other, &fov);
        CHECK(fov>=30 && fov<=36);
        float distance = 0;
        for (int k = 0; k < 9; k++)
            CHECK(isfinite(other[k]));
        for (int k = 0; k < 3; k++)
            distance += (other[k] - other[k + 3]) * (other[k] - other[k + 3]);
        CHECK(distance>.25f && other[1]>.3f && other[7]==1);
    }
    for (int shot = 0; shot < LAUNCHER_SHOTS; shot++) {
        float start = shot * LAUNCHER_SHOT_SECONDS;
        CHECK(launcher_scene_transition(start)==0);
        CHECK(fabsf(launcher_scene_transition(start+.75f)-.5f)<1e-5f);
        CHECK(launcher_scene_transition(start+1.5f)==1);
        CHECK(launcher_scene_transition(start+11.99f)==1);
        float previous = -1;
        for (int i = 0; i <= 150; i++) {
            float value = launcher_scene_transition(start + i * .01f);
            CHECK(value>=previous && value>=0 && value<=1);
            previous = value;
        }
    }
    char code[96];
    int color;
    CHECK(!launcher_scene_saved_car(fixture,code,&color));
    CHECK(!strcmp(code,"ford_gt_05") && color==0);
    gt4_save_init(fixture);
    CHECK(gt4_save_create(1,"Preview"));
    ah_vm *vm = ah_vm_new();
    ah_obj *game = ah_module_new(vm, "game", ah_root(vm), NULL, NULL);
    ah_obj *garage = ah_module_new(vm, "garage", NULL, NULL, NULL);
    ah_obj *cars = ah_array_new(vm), *car = ah_module_new(vm, "car", NULL, NULL, NULL);
    ah_set_member(vm, car, "car_code", ah_str(vm, "ka_01"));
    ah_set_member(vm, car, "color_index", ah_int(2));
    ah_array_push(vm, cars, ah_objv(car));
    ah_set_member(vm, garage, "cars", ah_objv(cars));
    ah_set_member(vm, garage, "riding_index", ah_int(0));
    ah_set_member(vm, game, "garage", ah_objv(garage));
    CHECK(gt4_save_capture(vm,game,1) && gt4_save_store());
    gt4_save_shutdown();
    CHECK(launcher_scene_saved_car(fixture,code,&color));
    CHECK(!strcmp(code,"ka_01") && color==2);
    if (argc > 3 && !strcmp(argv[2], "--disc")) {
        const char *error;
        gt4_vol *vol = gt4_vol_open(argv[3], &error);
        CHECK(vol);
        size_t n;
        uint8_t *labels = gt4_vol_read(vol, "/specdb/GT4_US2560/WHEEL.idi", &n);
        uint32_t id;
        CHECK(labels && gt4_specdb_label(labels,n,"brdg001w",&id));
        free(labels);
        uint8_t body[0x4b8];
        memset(body, 255, sizeof body);
        /* Fixture uses the serialized layout read by 00440938 and
           00454270/00454378; the second setup sheet is deliberately active. */
        uint64_t key = id;
        memcpy(body + 0x468, &key, 8);
        uint32_t sheet = 1;
        memcpy(body + 0x488, &sheet, 4);
        labels = gt4_vol_read(vol, "/specdb/GT4_US2560/WING.idi", &n);
        CHECK(labels && gt4_specdb_label(labels,n,"p001",&id));
        free(labels);
        key = id;
        memcpy(body + 0x178 + 0x100, &key, 8);
        CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body));
        gt4_save_init(fixture);
        CHECK(gt4_save_capture(vm,game,1) && gt4_save_store());
        gt4_save_shutdown();
        char wheel[32], wing[32];
        CHECK(launcher_scene_saved_vehicle(fixture,vol,code,&color,wheel,wing));
        CHECK(!strcmp(wheel,argc>4?"wrong-wheel":"brdg001w") && !strcmp(wing,"p001"));
        memset(body, 255, sizeof body);
        CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body));
        gt4_save_init(fixture);
        CHECK(gt4_save_capture(vm,game,1) && gt4_save_store());
        gt4_save_shutdown();
        CHECK(launcher_scene_saved_vehicle(fixture,vol,code,&color,wheel,wing));
        CHECK(!*wheel && !*wing);
        gt4_vol_close(vol);
    }
    gt4_save_init(fixture);
    ah_set_member(vm, garage, "riding_index", ah_int(5));
    CHECK(gt4_save_capture(vm,game,1) && gt4_save_store());
    gt4_save_shutdown();
    CHECK(!launcher_scene_saved_car(fixture,code,&color));
    CHECK(!strcmp(code,"ford_gt_05") && color==0);
    ah_vm_free(vm);
    puts("PASS saved-car selection, empty/invalid garage fallback and cinematic camera cycle");
    return 0;
}
