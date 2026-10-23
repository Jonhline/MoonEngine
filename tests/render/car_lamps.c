/* P: GPU cone/range/off-restore tests, same shader on both APIs. */
#include "render3d.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "car_lamps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"car lamps %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int invert_normal,cutoff_mode;
static int frame(int enabled,int aimed,float range,int hdr,unsigned char out[128*128*4]) {
    gt4_triangle t[2]={0};float points[6][3]={{-2,-2,3},{2,-2,3},{2,2,3},{-2,-2,3},{2,2,3},{-2,2,3}};
    for(int i=0;i<2;i++) {t[i].texture=-1;t[i].kind=1;t[i].state.depth_func=1;t[i].state.depth_write=1;
        for(int j=0;j<3;j++){memcpy(t[i].v[j].p,points[i*3+j],12);t[i].v[j].normal[2]=invert_normal?1:-1;
            for(int k=0;k<3;k++)t[i].v[j].rgb[k]=.1f;t[i].v[j].alpha=1;}}
    gt4_model m={0};m.triangles=t;m.count=2;r3d_mesh *mesh=r3d_mesh_create(&m);if(!mesh)return 0;
    r3d_car_lamp lamp={{0,0,1,range},{0,0,aimed?1:-1,.9f},{6,5.4f,4.5f,0}};
    lamp.color[3]=cutoff_mode?.02f:0;
    r3d_car_lamps(enabled);r3d_car_lamp_frame(&lamp,1);video_hdr_options(hdr,0);
    plat_video_begin_frame();if(hdr)video_begin_scene();
    r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=1;v.r[0]=v.u[1]=v.f[2]=1;v.focal=320;v.focal_y=224;v.unlit=1;v.color_scale=1;
    r3d_begin(0,0,640,448,NULL);r3d_draw(mesh,&v);r3d_end();if(hdr)video_end_scene();
    int w,h;uint8_t *p=plat_sdl_read_pixels(&w,&h);if(!p || w!=128 || h!=128){free(p);return 0;}
    memcpy(out,p,128*128*4);free(p);plat_video_end_frame();r3d_mesh_destroy(mesh);return 1;
}
int main(int argc,char **argv) {
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));SDL_HideWindow(plat_sdl_window());
    unsigned char off[65536],on[65536],away[65536],short_range[65536],restored[65536],hdr[65536];
    CHECK(frame(0,1,60,0,off));CHECK(frame(argc>1 && !strcmp(argv[1],"--negative")?0:1,1,60,0,on));
    CHECK(frame(1,0,60,0,away));CHECK(frame(1,1,1,0,short_range));CHECK(frame(0,1,60,0,restored));
    CHECK(!memcmp(off,away,sizeof off) && !memcmp(off,short_range,sizeof off) && !memcmp(off,restored,sizeof off));
    int center=(64*128+64)*4;printf("lamp center off=%d on=%d\n",off[center],on[center]);CHECK(on[center]>off[center]+10);
    CHECK(frame(1,1,60,1,hdr));CHECK(hdr[center]>off[center]);
    invert_normal=1;CHECK(frame(argc>1 && !strcmp(argv[1],"--normal-negative")?0:1,1,60,0,restored));invert_normal=0;
    CHECK(!memcmp(on,restored,sizeof on));
    cutoff_mode=!(argc>1 && !strcmp(argv[1],"--cutoff-negative"));CHECK(frame(1,1,60,0,restored));cutoff_mode=0;
    int below=(68*128+64)*4,above=(56*128+64)*4;
    printf("cutoff below=%d above=%d uncut=%d/%d base=%d/%d\n",restored[below],restored[above],on[below],on[above],off[below],off[above]);
    CHECK(restored[below]>off[below]+10 && restored[above]==off[above] && on[above]>off[above]+10);
    /* Model-coordinate emitters move with authored geometry, no per-car coordinates. */
    gt4_triangle triangles[4]={0};gt4_model model={0};model.triangles=triangles;model.count=4;
    for(int i=0;i<4;i++){triangles[i].kind=0;triangles[i].night_lens=1;triangles[i].texture=-1;
        for(int j=0;j<3;j++){gt4_vertex *v=&triangles[i].v[j];v->p[0]=(i%2?1.f:-1.f)+(j==1?.1f:0);v->p[1]=.5f+(j==2?.1f:0);v->p[2]=i<2?-2.f:2.f;v->normal[2]=i<2?-1:1;}}
    moon_car_lamp lamps[4];CHECK(moon_car_lamps(&model,lamps)==4);
    for(int i=0;i<4;i++)CHECK(fabsf(lamps[i].pos[2])==2 && lamps[i].direction[2]*(lamps[i].rear?1:-1)>.9f);
    plat_video_shutdown();puts("PASS cone/range/linear HDR/off restore and authored-coordinate lamp inference");return 0;
}
