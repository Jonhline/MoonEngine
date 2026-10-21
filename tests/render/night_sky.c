/* P: deterministic 24-hour presentation, independent of replay physics. */
#include "render3d.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "daylight.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"night sky %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int sky(float hour,unsigned char out[65536]) {
    gt4_triangle t[2]={0};float p[6][3]={{-8,-8,-3},{8,-8,-3},{8,8,-3},{-8,-8,-3},{8,8,-3},{-8,8,-3}};
    for(int i=0;i<2;i++){t[i].texture=-1;t[i].state.depth_func=1;t[i].state.depth_write=1;
        for(int j=0;j<3;j++){memcpy(t[i].v[j].p,p[i*3+j],12);t[i].v[j].alpha=1;}}
    gt4_model m={0};m.triangles=t;m.count=2;r3d_mesh *mesh=r3d_mesh_create(&m);if(!mesh)return 0;
    moon_daylight d=moon_night_at(hour);r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=1;v.r[0]=v.u[1]=1;v.f[2]=-1;
    v.focal=200;v.focal_y=180;v.daylight=2;v.unlit=1;v.color_scale=1;
    memcpy(v.sun_direction,d.sun,12);memcpy(v.sun_color,d.direct,12);memcpy(v.sky_color,d.sky,12);memcpy(v.haze_color,d.haze,12);
    plat_video_begin_frame();r3d_begin(0,0,640,448,NULL);r3d_draw(mesh,&v);r3d_end();
    int w,h;uint8_t *pixels=plat_sdl_read_pixels(&w,&h);if(!pixels||w!=128||h!=128){free(pixels);return 0;}
    memcpy(out,pixels,65536);free(pixels);plat_video_end_frame();r3d_mesh_destroy(mesh);return 1;
}
int main(int argc,char **argv) {
    r3d_night_sky(1);r3d_daylight_options(1,23,2);
    CHECK(r3d_solar_hour(120,0)==1 && r3d_solar_hour(120,1)==1);
    r3d_daylight_options(1,-1,0);CHECK(r3d_solar_hour(0,1)==22 && r3d_solar_hour(0,0)==12);
    r3d_daylight_options(1,12,0);CHECK(r3d_solar_hour(0,1)==12);
    r3d_daylight_options(1,23,0);CHECK(r3d_solar_hour(9000,0)==23);
    for(int i=0;i<2400;i++){
        moon_daylight a=moon_night_at(i*.01f),b=moon_night_at(i*.01f);
        CHECK(!memcmp(&a,&b,sizeof a));float norm=0;
        for(int c=0;c<3;c++){CHECK(isfinite(a.direct[c])&&a.direct[c]>=0&&a.sky[c]>=0);norm+=a.sun[c]*a.sun[c];}
        CHECK(fabsf(norm-1)<1e-5f);
        if(a.sun[1]<0)CHECK(a.direct[0]==0&&a.direct[1]==0&&a.direct[2]==0);
    }
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));SDL_HideWindow(plat_sdl_window());r3d_night_sky(1);
    unsigned char noon[65536],night[65536],repeat[65536],dawn[65536];
    CHECK(sky(12,noon));CHECK(sky(argc>1?12:0,night));CHECK(sky(0,repeat));CHECK(sky(6,dawn));
    unsigned long day_sum=0,night_sum=0;int stars=0;
    for(int i=0;i<128*128;i++){day_sum+=noon[i*4]+noon[i*4+1]+noon[i*4+2];night_sum+=night[i*4]+night[i*4+1]+night[i*4+2];stars+=night[i*4]>40;}
    printf("sky day=%lu night=%lu stars=%d\n",day_sum,night_sum,stars);
    CHECK(night_sum<day_sum/8 && stars>0);CHECK(!memcmp(night,repeat,sizeof night));
    r3d_night_sky(0);r3d_daylight_options(1,0,0);CHECK(r3d_solar_hour(0,1)==7);r3d_daylight_options(1,16,2);CHECK(r3d_daylight_hour(120)==16);
    plat_video_shutdown();puts("PASS full-day cycle, dark night/star field, below-horizon sun, repeatable sky, legacy off");return 0;
}
