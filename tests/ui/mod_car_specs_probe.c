/* Read the same native builders used by the UI, then an actual race.
   Including the existing vehicle adapter exposes only test-private RAM. */
#include "../../src/core/career_vehicle.c"
#include "arcade.h"
#include "physics/race_run.h"
#include <stdio.h>
#include <math.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"spec probe %d: %s\n",__LINE__,#x);return 1; } } while(0)
static int silent_read(void *ctx,uint32_t car,uint8_t *out,const uint8_t *events)
{
    (void)ctx;(void)car;(void)events;memset(out,0,32);return 1;
}
static int silent_wheel(void *ctx,uint32_t car,int16_t state[2])
{
    (void)ctx;(void)car;state[0]=state[1]=1;return 1;
}
static int silent_write(void *ctx,uint32_t car,const uint8_t *out)
{
    (void)ctx;(void)car;(void)out;return 1;
}

int main(int argc, char **argv)
{
    CHECK(argc>=3);
    gt4_career_vehicle *v=gt4_career_vehicle_open(argv[1]);CHECK(v);
    uint8_t unit[32]={0},body[0x4b8];int32_t stats[3];gt4_setting_details details={0};
    CHECK(gt4_career_vehicle_create(v,argv[2],0,0,unit,body));
    CHECK(gt4_career_vehicle_stats(v,body,stats));
    CHECK(gt4_career_vehicle_setting(v,body,2,-1,&details));
    printf("native UI %s: PS=%.3f Nm=%.3f kg=%d redline=%.0f\n",argv[2],
           details.peak_power,details.peak_torque*9.8,stats[2],details.max_rpm);
    gt4_phys_mem=v->ram;
    uint32_t drive=REC+0x834;
    printf("native params: drive=%u gears=%u mass=%.3f\n",GT4_U8(drive),GT4_U8(drive+2),GT4_F32(REC+0x600+PARAMS_MASS));
    uint8_t raw[256];CHECK(v->db.row(v->db.ctx,10,r32(body+0x40),0,raw,sizeof raw)==21);
    printf("catalog drivetrain code=%u\n",raw[0x10]);
    uint32_t car_id;CHECK(gt4_specdb_label(v->labels,v->labels_size,argv[2],&car_id));
    gt4_phys_mem=NULL;gt4_career_vehicle_close(v);
    if (argc>3 && !strcmp(argv[3],"--race")) {
        gt4_race_desc desc={0};strcpy(desc.course,"tsukuba");desc.cars=1;desc.car[0]=car_id;desc.bspec=1;
        char message[256];gt4_race_run *run=gt4_race_run_open(argv[1],&desc,message,sizeof message);
        if (!run) fprintf(stderr,"%s\n",message);
        CHECK(run);
        gt4_phys_pad=(gt4_phys_pads){NULL,silent_read,silent_wheel,silent_write};
        uint32_t car=gt4_race_run_car(run,0),params=GT4_U32(car+CAR_PARAMS),d=params+PARAMS_DRIVE;
        printf("race fields: drive=%u gears=%u redline=%u mass=%.3f\n",GT4_U8(d+DRIVE_KIND),GT4_U8(d+2),
               GT4_U16(params+PARAMS_ENGINE+ENGINE_REDLINE),GT4_F32(params+PARAMS_MASS));
        for (unsigned i=1;i<=GT4_U8(d+2);i++) {
            float ratio=GT4_F32(d+DRIVE_RATIOS+i*4),radius=GT4_F32(d+DRIVE_RADIUS+GT4_U8(d+3)*4);
            printf("race gear %u: overall=%.6f speed_at_3000rpm=%.3f km/h\n",i,ratio,3000*6.28318530718/60*radius/ratio*3.6);
        }
        for (unsigned i=0;i<1200;i++) {
            CHECK(gt4_race_run_frame(run));
            if (i%240==239) printf("race tick %u: RPM=%u gear=%u speed=%.3f m/s\n",i+1,GT4_U16(car+CAR_ENGINE_RPM),GT4_U8(car+CAR_GEAR),GT4_F32(car+CAR_SPEED));
        }
        CHECK(isfinite(GT4_F32(car+CAR_SPEED)));
        gt4_race_run_close(run);
        memset(&gt4_phys_pad,0,sizeof gt4_phys_pad);
    }
    return 0;
}
