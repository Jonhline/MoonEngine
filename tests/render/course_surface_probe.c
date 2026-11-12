/* Local read-only course survey. Captures/assets remain outside the repo. */
#include "../../src/platform/sdl/race_view.c"
#include "miniz.h"
#include "plat_sdl.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"surface probe %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int shot(race_view *v,const char *path) {
    plat_video_begin_frame();video_begin_scene();race_view_draw(v);video_end_scene();
    int w,h;size_t n;uint8_t *p=plat_sdl_read_pixels(&w,&h);
    void *png=p?tdefl_write_image_to_png_file_in_memory(p,w,h,4,&n):NULL;
    FILE *f=png?fopen(path,"wb"):NULL;
    if(f){fwrite(png,1,n,f);fclose(f);}free(p);mz_free(png);plat_video_end_frame();return f!=NULL;
}
int main(int argc,char **argv) {
    CHECK(argc==4 || argc==10);
    CHECK(!plat_video_init(&(plat_video_desc){960,720,0,0,0}));SDL_HideWindow(plat_sdl_window());
    video_scene_quality(1,2);
    race_view *v=race_view_open(argv[1]);CHECK(v);
    CHECK(race_view_request_film(v,argv[2],"focus_rs_02",0));
    int ready=0;for(int i=0;i<4000 && !ready;i++){SDL_Delay(5);ready=race_view_poll(v,0);}CHECK(ready==1);
    race_asset *a=v->now;gt4_model_stats stats={0};
    gt4_model *m=gt4_model_decode_world(a->data+a->course.world,a->course.size-a->course.world,&stats);CHECK(m);
    printf("SURVEY triangles=%d unsupported=%d fail_op=%d offset=%zu\n",m->count,m->unsupported,stats.fail_op,stats.fail_offset);
    for(int i=0;i<256;i++)if(stats.callbacks[i])printf("CALLBACK id=%d count=%u\n",i,stats.callbacks[i]);
    for(int i=0;i<m->count;i++) {
        gt4_triangle *t=m->triangles+i;float *p=t->v[0].p,*q=t->v[1].p,*r=t->v[2].p;
        float area=fabsf((q[0]-p[0])*(r[2]-p[2])-(q[2]-p[2])*(r[0]-p[0]))*.5f;
        if(area>100 && fabsf(p[1]-q[1])<.01 && fabsf(p[1]-r[1])<.01)
            printf("PLANE model=%u tex=%d kind=%u blend=%d/%u area=%g center=%g,%g,%g rgb=%g,%g,%g\n",t->source_model,t->texture,t->kind,t->blend_enabled,t->state.blend,area,(p[0]+q[0]+r[0])/3,p[1],(p[2]+q[2]+r[2])/3,t->v[0].rgb[0],t->v[0].rgb[1],t->v[0].rgb[2]);
    }
    for(int i=0;i<m->count;i++) {
        gt4_triangle *t=m->triangles+i;
        float *p=t->v[0].p,*q=t->v[1].p,*r=t->v[2].p;
        float area=fabsf((q[0]-p[0])*(r[2]-p[2])-(q[2]-p[2])*(r[0]-p[0]))*.5f;
        if(area>1000 && p[1]<-2 && fabsf(p[1]-q[1])<.01 && fabsf(p[1]-r[1])<.01)
            printf("LOW_PLANE model=%u tex=%d points=%g,%g,%g / %g,%g,%g / %g,%g,%g\n",t->source_model,t->texture,p[0],p[1],p[2],q[0],q[1],q[2],r[0],r[1],r[2]);
    }
    if(getenv("MOON_PROBE_ROOFS"))for(int i=0;i<m->count;i++) {
        gt4_triangle *t=m->triangles+i;float center[3]={0},e[3],f[3],normal[3];
        for(int k=0;k<3;k++){
            for(int j=0;j<3;j++)center[k]+=t->v[j].p[k]/3;
            e[k]=t->v[1].p[k]-t->v[0].p[k];f[k]=t->v[2].p[k]-t->v[0].p[k];
        }
        for(int k=0;k<3;k++)normal[k]=e[(k+1)%3]*f[(k+2)%3]-e[(k+2)%3]*f[(k+1)%3];float size=sqrtf(dot3(normal,normal)),ground;
        if(size>20 && fabsf(normal[1])>size*.8f && gt4_course_height(&a->course,center[0],center[2],center[1]-1,&ground) && center[1]-ground>2 && center[1]-ground<12)
            printf("ROOF center=%g,%g,%g ground=%g area=%g model=%u\n",center[0],center[1],center[2],ground,size*.5f,t->source_model);
    }
    gt4_model_free(m);
    gt4_model *far=course_set(a->data,a->course.size,0x58);
    if(far) {
        for(int i=0;i<far->count;i++){
            gt4_triangle *t=far->triangles+i;float *p=t->v[0].p,*q=t->v[1].p,*r=t->v[2].p;
            float area=fabsf((q[0]-p[0])*(r[2]-p[2])-(q[2]-p[2])*(r[0]-p[0]))*.5f;
            if(area>1000 && fabsf(p[1]-q[1])<.01 && fabsf(p[1]-r[1])<.01)
                printf("FAR_PLANE model=%u tex=%d y=%g area=%g center=%g,%g\n",t->source_model,t->texture,p[1],area,(p[0]+q[0]+r[0])/3,(p[2]+q[2]+r[2])/3);
        }
        gt4_model_free(far);
    }
    if(getenv("MOON_PROBE_ALL_MODELS"))memset(a->vision,0,sizeof a->vision);

    float camera[3]={100,500,650},target[3]={100,0,550};
    if(argc==10)for(int k=0;k<3;k++){camera[k]=(float)atof(argv[4+k]);target[k]=(float)atof(argv[7+k]);}
    const char *car=getenv("MOON_PROBE_CAR");float pose[4];
    if(car && sscanf(car,"%f,%f,%f,%f",pose,pose+1,pose+2,pose+3)==4) {
        float c=cosf(pose[3]),s=sinf(pose[3]),rot[9]={c,0,s,0,1,0,-s,0,c};
        race_view_set_pose(v,pose,rot);
    }
    r3d_view eye={0};look_at(&eye,camera,target);float rows[9];
    for(int k=0;k<3;k++){rows[k]=eye.r[k];rows[3+k]=eye.u[k];rows[6+k]=-eye.f[k];}
    race_view_set_camera(v,camera,rows,1.1f,1.433f,.1f);
    char path[1024];
    for(int mode=0;mode<3;mode++) {
        video_hdr_options(mode>0 && getenv("MOON_PROBE_HDR"),0);
        r3d_modern_lighting(mode>0);r3d_daylight_options(mode>0,-1,0);r3d_night_sky(mode>0);r3d_car_lamps(0);
        if(mode==2)SDL_setenv("MOON_NO_SHEEN","1",1);
        snprintf(path,sizeof path,"%s-%s.png",argv[3],mode==0?"classic":mode==1?"modern":"no-sheen");CHECK(shot(v,path));
    }
    const char *header=getenv("MOON_PROBE_HEADER");
    if(header) {
        gt4_model *extra=course_set(a->data,a->course.size,(unsigned)strtoul(header,NULL,0));CHECK(extra);
        float lo[3]={1e9f,1e9f,1e9f},hi[3]={-1e9f,-1e9f,-1e9f};
        for(int i=0;i<extra->count;i++)for(int j=0;j<3;j++)for(int k=0;k<3;k++){
            float f=extra->triangles[i].v[j].p[k];lo[k]=fminf(lo[k],f);hi[k]=fmaxf(hi[k],f);
        }
        printf("HEADER %s triangles=%d bounds=%g,%g,%g to %g,%g,%g\n",header,extra->count,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
        r3d_mesh *saved_world=v->world,*saved_far=v->far;
        v->world=r3d_mesh_create(extra);v->far=NULL;memset(a->vision,0,sizeof a->vision);
        r3d_modern_lighting(0);r3d_daylight_options(0,-1,0);r3d_night_sky(0);
        snprintf(path,sizeof path,"%s-header.png",argv[3]);CHECK(shot(v,path));
        r3d_mesh_destroy(v->world);v->world=saved_world;v->far=saved_far;gt4_model_free(extra);
    }
    race_view_close(v);plat_video_shutdown();return 0;
}
