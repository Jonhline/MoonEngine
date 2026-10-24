/* P: deterministic cloud volume and sun/sky separation; not a PS2 sky oracle. */
#include "plat_sdl.h"
#undef main
#define main daylight_fixture_main
#include "daylight.c"
#undef main
#ifdef _WIN32
#define main SDL_main
#endif
#include "miniz.h"
static int authored_fixture,night_fixture;
static int clouds(int enabled,float seconds,uint8_t out[128*128*4]) {
    gt4_triangle triangles[2]={0};
    const float points[6][3]={{-3,3,3},{3,3,3},{3,3,-3},{-3,3,3},{3,3,-3},{-3,3,-3}};
    for(int i=0;i<2;i++){triangles[i].texture=-1;triangles[i].state.depth_func=1;triangles[i].state.depth_write=1;
      for(int j=0;j<3;j++){memcpy(triangles[i].v[j].p,points[i*3+j],12);triangles[i].v[j].alpha=1;for(int c=0;c<3;c++)triangles[i].v[j].rgb[c]=c==0?.15f:c==1?.25f:.4f;}}
    gt4_model model={.triangles=triangles,.count=2};r3d_mesh *mesh=r3d_mesh_create(&model);if(!mesh)return 0;
    r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=1;v.r[0]=v.f[1]=1;v.u[2]=-1;v.focal=320;v.focal_y=224;v.unlit=1;v.daylight=2;v.color_scale=255.f/128;v.authored_environment=authored_fixture;v.sky_seconds=seconds;
    moon_daylight sun=night_fixture?moon_night_at(22):moon_daylight_at(12);memcpy(v.sun_direction,sun.sun,12);memcpy(v.sun_color,sun.direct,12);memcpy(v.sky_color,sun.sky,12);
    r3d_night_sky(enabled);r3d_begin(0,0,640,448,NULL);if(!r3d_begin_target(128))return 0;r3d_draw(mesh,&v);r3d_end_target(out);r3d_mesh_destroy(mesh);return 1;
}
int main(int argc,char **argv){
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));SDL_HideWindow(plat_sdl_window());
    uint8_t clear[128*128*4],cloudy[128*128*4],repeat[128*128*4],wind[128*128*4],restored[128*128*4];
    CHECK(clouds(0,0,clear));CHECK(clouds(argc>1 && !strcmp(argv[1],"--negative")?0:1,0,cloudy));CHECK(clouds(1,0,repeat));CHECK(clouds(1,300,wind));CHECK(clouds(0,0,restored));
    unsigned changed=0,moved=0;for(unsigned i=0;i<sizeof clear;i++){changed+=clear[i]!=cloudy[i];moved+=wind[i]!=repeat[i];}
    printf("cloud channels changed=%u moved=%u\n",changed,moved);
    CHECK(changed>1000);CHECK(moved>1000);CHECK(!memcmp(cloudy,repeat,sizeof cloudy));CHECK(!memcmp(clear,restored,sizeof clear));
    uint8_t reference[128*128*4];
    SDL_setenv("MOON_CLOUD_REFERENCE","1",1);
    CHECK(clouds(1,argc>1 && !strcmp(argv[1],"--cache-negative")?300:0,reference));
    SDL_setenv("MOON_CLOUD_REFERENCE","",1);
    unsigned total_error=0,max_error=0,large=0;
    for(unsigned i=0;i<sizeof reference;i++){unsigned e=abs((int)reference[i]-cloudy[i]);total_error+=e;if(e>max_error)max_error=e;large+=e>8;}
    printf("cloud cache error mean=%.4f max=%u channels over8=%u\n",(double)total_error/sizeof reference,max_error,large);
    CHECK((double)total_error/sizeof reference<1.5);CHECK(large<sizeof reference/100);
    /* Authored reference irradiance does not retain the source sky texture.
       Both paths must show the same full procedural sky, not two cloud layers. */
    authored_fixture=1;
    CHECK(clouds(argc>1 && !strcmp(argv[1],"--authored-negative")?0:1,0,reference));
    CHECK(!memcmp(reference,cloudy,sizeof reference));
    night_fixture=!(argc>1 && !strcmp(argv[1],"--night-negative"));
    CHECK(clouds(1,0,reference));
    unsigned long night_sum=0,day_sum=0;
    for(unsigned i=0;i<sizeof reference;i++)if(i%4!=3){night_sum+=reference[i];day_sum+=cloudy[i];}
    printf("night/day sky sums %lu/%lu\n",night_sum,day_sum);
    CHECK(night_sum<day_sum/5);
    const char *api=getenv("MOON_GRAPHICS_API");char *base=SDL_GetBasePath();char path[1024];snprintf(path,sizeof path,"%s../realistic-sky-%s.png",base,api?api:"opengl");SDL_free(base);
    size_t bytes;void *png=tdefl_write_image_to_png_file_in_memory(cloudy,128,128,4,&bytes);CHECK(png);FILE *f=fopen(path,"wb");CHECK(f && fwrite(png,1,bytes,f)==bytes);fclose(f);mz_free(png);
    plat_video_shutdown();printf("PASS cloud volume changed %u channels, deterministic wind changed %u, disabled exact\n",changed,moved);return 0;
}
