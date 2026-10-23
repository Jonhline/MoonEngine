/* Read-only authored night-course GPU audit. Disc images stay under build/. */
#include "../../src/platform/sdl/race_view.c"
#include "miniz.h"
#include "plat_sdl.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"night courses %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint64_t fingerprint;
static int capture(race_view *v,const char *path,int pregrid) {
    plat_video_begin_frame();
    if(pregrid) race_view_draw_pregrid(v);else race_view_draw(v);
    int w,h;size_t n;uint8_t *p=plat_sdl_read_pixels(&w,&h);
    fingerprint=1469598103934665603ull;
    if(p) for(int i=0;i<w*h;i++) for(int c=0;c<3;c++) fingerprint=(fingerprint^p[i*4+c])*1099511628211ull;
    void *png=p?tdefl_write_image_to_png_file_in_memory(p,w,h,4,&n):NULL;
    FILE *f=png?fopen(path,"wb"):NULL;
    if(f){fwrite(png,1,n,f);fclose(f);}free(p);mz_free(png);
    plat_video_end_frame();return f!=NULL;
}
int main(int argc,char **argv) {
    CHECK(argc>=4); /* disc, course, output prefix [--negative] */
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    r3d_daylight_options(0,12,0);r3d_modern_lighting(0);r3d_car_shadows(0);r3d_shadow_map_quality(0);
    race_view *v=race_view_open(argv[1]);CHECK(v);
    const char *car = getenv("NIGHT_CAPTURE_CAR");
    CHECK(race_view_request_film(v,argv[2],car && *car?car:"focus_rs_02",0));
    int ready=0;
    for(int i=0;i<4000 && !ready;i++){SDL_Delay(5);ready=race_view_poll(v,0);}CHECK(ready==1 && v->now);
    race_asset *a=v->now;
    CHECK(a->fog && a->glow_alpha>=0);
    CHECK(a->car && v->mine.glow && v->sky_mesh);
    gt4_grid grid=gt4_course_grid(&a->course,0);
    if(!a->course.grid_count){gt4_photo_location loc;CHECK(gt4_photo_location_read(a->data,a->course.size,&loc));
        grid.pos[0]=loc.car[0];grid.pos[1]=loc.car[1];grid.pos[2]=loc.car[2];grid.heading=loc.car_angle;}
    float rot[9]={cosf(grid.heading),0,sinf(grid.heading),0,1,0,-sinf(grid.heading),0,cosf(grid.heading)};
    race_view_set_pose(v,grid.pos,rot);
    float ground;int tri=-1;
    gt4_course_surface_triangle(a->surface,grid.pos[0],grid.pos[2],grid.pos[1]+5,&ground,NULL,&tri);
    r3d_lights lighting;race_lamp_shadows shadows;
    CHECK(race_lights_sample(&a->course,tri,grid.pos,1,1,&lighting));
    CHECK(race_lights_shadows(&a->course,tri,grid.pos,NULL,0,&shadows));
    char path[1024];
    uint64_t authored=0;
    for(int angle=0;angle<2;angle++) {
        r3d_view eye={0};float target[3]={grid.pos[0],grid.pos[1]+.7f,grid.pos[2]};
        float camera[3];
        for(int k=0;k<3;k++) camera[k]=grid.pos[k]+rot[k*3+2]*(angle? -7.f:7.f)+rot[k*3]*(angle?3.f:0);
        camera[1]+=2;look_at(&eye,camera,target);
        float rows[9];for(int k=0;k<3;k++){rows[k]=eye.r[k];rows[3+k]=eye.u[k];rows[6+k]=-eye.f[k];}
        race_view_set_camera(v,camera,rows,1.73205f,2.25378f,.1f);
        snprintf(path,sizeof path,"%s-%s.png",argv[3],angle?"lamps":"race");CHECK(capture(v,path,0));
        if(angle) authored=fingerprint;
    }
    snprintf(path,sizeof path,"%s-pregrid.png",argv[3]);CHECK(capture(v,path,1));
    /* Original night stays authored when a day-only procedural renderer is enabled. */
    r3d_daylight_options(1,12,0);
    int night=a->time_zone;
    if(argc>4) a->time_zone=0; /* negative recreates the daytime replacement */
    snprintf(path,sizeof path,"%s-daylight.png",argv[3]);CHECK(capture(v,path,0));
    CHECK(!night || authored==fingerprint); /* A day-only sky may never replace authored night. */
    r3d_daylight_options(0,12,0);
    r3d_car_lamps(1);
    snprintf(path,sizeof path,"%s-enhanced-lamps.png",argv[3]);CHECK(capture(v,path,0));
    printf("lamp geometry count=%u\n",a->lamp_count[0]);
    if(a->time_zone && a->lamp_count[0])CHECK(fingerprint!=authored);
    for(unsigned i=0;i<a->lamp_count[0];i++)printf("emitter rear=%d position=%g,%g,%g direction=%g,%g,%g\n",a->lamps[0][i].rear,a->lamps[0][i].pos[0],a->lamps[0][i].pos[1],a->lamps[0][i].pos[2],a->lamps[0][i].direction[0],a->lamps[0][i].direction[1],a->lamps[0][i].direction[2]);
    r3d_night_sky(1);r3d_daylight_options(1,-1,0);r3d_car_lamps(1);
    snprintf(path,sizeof path,"%s-enhanced-night.png",argv[3]);CHECK(capture(v,path,0));
    if(getenv("NIGHT_CAPTURE_CYCLE")) {
        r3d_daylight_options(1,0,2);
        for(int frame=0;frame<=96;frame++) {
            race_view_daylight_time(v,frame*15.);
            snprintf(path,sizeof path,"%s-cycle-%03d.png",argv[3],frame);
            CHECK(capture(v,path,0));
        }
        const int hours[]={8,12,16,20,0,4};
        for(int sample=0;sample<6;sample++) {
            moon_daylight d=moon_night_at((float)hours[sample]);
            const float *direction=sample<3?d.sun:d.moon;
            float camera[3]={grid.pos[0],grid.pos[1]+2,grid.pos[2]},target[3];
            for(int k=0;k<3;k++)target[k]=camera[k]+direction[k]*100;
            r3d_view eye={0};look_at(&eye,camera,target);
            float rows[9];for(int k=0;k<3;k++){rows[k]=eye.r[k];rows[3+k]=eye.u[k];rows[6+k]=-eye.f[k];}
            race_view_set_camera(v,camera,rows,1.73205f*4,2.25378f*4,.1f);
            race_view_daylight_time(v,hours[sample]*60.);
            snprintf(path,sizeof path,"%s-%s-%02d.png",argv[3],sample<3?"sun":"moon",hours[sample]);
            CHECK(capture(v,path,0));
        }
    }
    r3d_night_sky(0);r3d_daylight_options(0,12,0);r3d_car_lamps(0);
    printf("AUDIT %s zone=%d sky=%d fog=%g,%g,%g distance=%g floor=%g glow=%g lights=%d lamp_shadows=%d triangle=%d\n",
        argv[2],a->time_zone,v->sky_mesh!=NULL,a->fog_rgb[0],a->fog_rgb[1],a->fog_rgb[2],a->fog_distance,a->fog_max,
        a->glow_alpha,lighting.count,shadows.count,tri);
    race_view_close(v);plat_video_shutdown();return 0;
}
