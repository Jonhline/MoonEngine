/* P: bounded emitter survey using the actual night race models. No GPU/physics. */
#include "car_model.h"
#include "car_lamps.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "lamp survey %d: %s\n", __LINE__, #x);                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    car_model_tables *tables = car_model_tables_open(vol);
    CHECK(tables);
    const char *cars[] = {"focus_rs_02",
                          "mustang_gt_05",
                          "svt_f150_lightning_03",
                          "gt40_race_69",
                          "beat_91",
                          "ka_01",
                          "crx_sir_90",
                          "_787b_91",
                          "r92cp_92",
                          "r390_race_98",
                          "gt_one_race_exxon_99",
                          "r34_gtr_v_gtcolor_pc_01"};
    int missing = 0;
    for (unsigned c = 0; c < sizeof cars / sizeof *cars; c++) {
        car_model_tables_time_zone(tables, 1);
        car_model *model = car_model_load_race(vol, tables, cars[c], 0, 6, c % 6);
        CHECK(model && model->car);
        if (argc > 2)
            for (int i = 0; i < model->car->count; i++) {
                model->car->triangles[i].night_lens = 0;
                model->car->triangles[i].lamp = 0;
            }
        if (getenv("LAMP_SURVEY_DUMP"))
            for (int i = 0; i < model->car->count; i++) {
                const gt4_triangle *t = &model->car->triangles[i];
                float p[3] = {0}, n[3] = {0};
                for (int j = 0; j < 3; j++)
                    for (int k = 0; k < 3; k++) {
                        p[k] += t->v[j].p[k] / 3;
                        n[k] += t->v[j].normal[k] / 3;
                    }
                if (p[2] > .8 && (t->night_lens || t->lamp == 2))
                    printf("CAND %s kind=%d lamp=%d night=%d attachment=%d p=%g,%g,%g n=%g,%g,%g\n",
                           cars[c], t->kind, t->lamp, t->night_lens, t->attachment, p[0], p[1],
                           p[2], n[0], n[1], n[2]);
            }
        moon_car_lamp lamps[4];
        unsigned count = moon_car_lamps(model->car, lamps), front = 0, rear = 0;
        for (unsigned i = 0; i < count; i++) {
            float norm = 0;
            for (int k = 0; k < 3; k++) {
                CHECK(isfinite(lamps[i].pos[k]) && isfinite(lamps[i].direction[k]));
                norm += lamps[i].direction[k] * lamps[i].direction[k];
            }
            CHECK(fabsf(norm-1)<1e-4f);
            CHECK(lamps[i].direction[2]*(lamps[i].rear?1:-1)>0);
            if (lamps[i].rear)
                rear++;
            else
                front++;
            printf("EMITTER %s %u rear=%d p=%g,%g,%g d=%g,%g,%g\n", cars[c], i, lamps[i].rear,
                   lamps[i].pos[0], lamps[i].pos[1], lamps[i].pos[2], lamps[i].direction[0],
                   lamps[i].direction[1], lamps[i].direction[2]);
        }
        printf("SURVEY %s front=%u rear=%u triangles=%d\n", cars[c], front, rear,
               model->car->count);
        fflush(stdout);
        missing += front != 2 || rear != 2;
        car_model_free(model);
        /* Day cars retain pressed brake geometry even without a night branch. */
        int safety = !strcmp(cars[c], "r34_gtr_v_gtcolor_pc_01");
        car_model_tables_time_zone(tables, safety ? 1 : 0);
        model = car_model_load_race(vol, tables, cars[c], 0, 6, c % 6);
        CHECK(model && model->car);
        count = moon_car_lamps(model->car, lamps);
        front = rear = 0;
        for (unsigned i = 0; i < count; i++)
            if (lamps[i].rear)
                rear++;
            else
                front++;
        printf("DAY_SURVEY %s front=%u rear=%u safety=%d\n", cars[c], front, rear, safety);
        missing += rear != 2 || (safety && front != 2);
        car_model_free(model);
    }
    car_model_tables_close(tables);
    gt4_vol_close(vol);
    CHECK(!missing);
    puts("PASS twelve night race models have paired, finite, normalized emitters");
    return 0;
}
