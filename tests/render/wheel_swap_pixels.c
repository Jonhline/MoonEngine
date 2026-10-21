/* Local user-disc diagnostic: exercise repeated asynchronous wheel replacement. */
#include <SDL.h>
#undef main
#define main showroom_probe_original_main
#include "../ui/showroom_probe.c"
#undef main
typedef struct {char names[512][32];unsigned n;} wheel_names;
static void collect_wheel(void *user,const char *name,int directory,uint32_t size)
{
    (void)size;wheel_names *w=user;size_t n=strlen(name);
    if(!directory && n && name[n-1]=='w' && n<32 && w->n<512)strcpy(w->names[w->n++],name);
}
static int capture_wheel(showroom *s,const char *car,const char *out,const char *wheel)
{
    showroom_set_wheel(s,wheel);
    int ready=0;
    for(int i=0;i<2000 && !ready;i++){
        SDL_PumpEvents();SDL_Delay(1);
        plat_video_begin_frame();video_begin_scene();ready=showroom_draw(s,car,.05f);video_end_scene();plat_video_end_frame();
        if(ready<0)return 0;
    }
    if(!ready)return 0;
    plat_video_begin_frame();video_begin_scene();showroom_draw(s,car,0);video_end_scene();
    int w,h;size_t n=0;uint8_t *px=plat_sdl_read_pixels(&w,&h);
    void *png=px?tdefl_write_image_to_png_file_in_memory(px,w,h,4,&n):NULL;
    char path[512];snprintf(path,sizeof path,"%s/%s.png",out,wheel);
    FILE *f=png?fopen(path,"wb"):NULL;
    if(f){fwrite(png,1,n,f);fclose(f);}
    mz_free(png);free(px);plat_video_end_frame();return f!=NULL;
}
int main(int argc,char **argv)
{
    if(argc<4)return 2;
    gt4_vol *vol=gt4_vol_open(argv[1],NULL);if(!vol)return 1;
    if(getenv("WHEEL_PROBE_INSPECT")){
        car_model_tables *t=car_model_tables_open(vol);
        for(int mode=0;mode<2;mode++){
            car_model *a=mode?car_model_load_parts(vol,t,argv[2],2,"bbs_001w",NULL):car_model_load_race_parts(vol,t,argv[2],2,6,0,"bbs_001w",NULL);
            unsigned zero[6]={0}, shine[6]={0}, bottom[6]={0};
            if(!a)return 1;
            for(int i=0;i<a->body_count;i++)for(int j=0;j<3;j++){
                const gt4_triangle *tr=&a->car->triangles[i];const gt4_vertex *v=&tr->v[j];unsigned k=tr->kind<6?tr->kind:0;
                float n=v->normal[0]*v->normal[0]+v->normal[1]*v->normal[1]+v->normal[2]*v->normal[2];
                zero[k]+=n<1e-12;shine[k]+=v->shine<=0;bottom[k]+=v->p[1]<.4f && v->shine<=0;
            }
            printf("%s %s\n",argv[2],mode?"menu":"race");
            for(int k=0;k<6;k++)printf("kind %d: zero normals %u zero shine %u bottom zero shine %u\n",k,zero[k],shine[k],bottom[k]);
            car_model_free(a);
        }
        car_model_tables_close(t);gt4_vol_close(vol);return 0;
    }
    wheel_names wheels={0};gt4_vol_list(vol,"/wheel/lod",collect_wheel,&wheels);gt4_vol_close(vol);
    plat_video_desc desc={960,672,0,0,0};if(plat_video_init(&desc))return 1;
    plat_video_set_view(640,448);
    if(getenv("WHEEL_PROBE_AA"))video_scene_quality(atoi(getenv("WHEEL_PROBE_AA")),2);
    if(getenv("WHEEL_PROBE_FSR"))video_set_upscale(atoi(getenv("WHEEL_PROBE_FSR")));
    if(getenv("WHEEL_PROBE_AO")){video_hdr_options(1,0);video_ambient_occlusion(1);}
    showroom *s=showroom_open_studio(argv[1]);if(!s)return 1;
    int shade=getenv("WHEEL_PROBE_BG")?atoi(getenv("WHEEL_PROBE_BG")):0;
    const uint8_t black[3]={(uint8_t)shade,(uint8_t)shade,(uint8_t)shade};showroom_set_picture(s,1,black);
    float eye[3]={10,.45f,0},target[3]={0,.5f,0},offset[3]={0,0,0};
    if(getenv("WHEEL_PROBE_ORIGIN")){
        float origin=(float)atof(getenv("WHEEL_PROBE_ORIGIN"));
        eye[0]+=origin;eye[2]+=origin;target[0]+=origin;target[2]+=origin;
        offset[0]+=origin;offset[2]+=origin;
    }
    if(getenv("WHEEL_PROBE_MATERIAL"))r3d_material_shading(1);
    showroom_set_rig(s,eye,target,offset,getenv("WHEEL_PROBE_ANGLE")?(float)atof(getenv("WHEEL_PROBE_ANGLE")):0);showroom_set_selection(s,argv[2],2);
    if(!capture_wheel(s,argv[2],argv[3],"w000"))return 1;
    unsigned limit=argc>4?(unsigned)atoi(argv[4]):wheels.n;
    for(unsigned i=0;i<wheels.n && i<limit;i++){
        if(!capture_wheel(s,argv[2],argv[3],wheels.names[i]))return 1;
        printf("%s\n",wheels.names[i]);fflush(stdout);
    }
    showroom_close(s);plat_video_shutdown();return 0;
}
