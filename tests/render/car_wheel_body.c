/* Drawing-data regression: GT Auto rims must not modify race body materials.
   No physics or PS2 pixel-equivalence claim. User-disc assets remain local. */
#include "car_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"wheel body line %d: %s\n",__LINE__,#x);return 1;} }while(0)
typedef struct { char names[512][32]; unsigned count; } rims;
static void collect(void *data,const char *name,int directory,uint32_t size)
{
    (void)size;rims *r=data;size_t n=strlen(name);
    if(!directory && n && name[n-1]=='w' && n<32 && r->count<512)
        strcpy(r->names[r->count++],name);
}
static int same_body(const car_model *a,const car_model *b)
{
    if(a->body_count!=b->body_count)return 0;
    for(int i=0;i<a->body_count;i++){
        const gt4_triangle *x=&a->car->triangles[i],*y=&b->car->triangles[i];
        if(memcmp(x->v,y->v,sizeof x->v) || x->texture!=y->texture || x->kind!=y->kind ||
           memcmp(&x->state,&y->state,sizeof x->state) || x->attachment!=y->attachment ||
           x->blend_enabled!=y->blend_enabled || x->lamp!=y->lamp)return 0;
        if(x->texture>=0){
            if(x->texture>=a->car->ntextures || y->texture>=b->car->ntextures)return 0;
            const tex1_image *at=&a->car->textures[x->texture],*bt=&b->car->textures[y->texture];
            if(at->width!=bt->width || at->height!=bt->height ||
               memcmp(at->rgba,bt->rgba,(size_t)at->width*at->height*4))return 0;
        }
    }
    return 1;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2);gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol);
    car_model_tables *tables=car_model_tables_open(vol);CHECK(tables);
    rims r={0};gt4_vol_list(vol,"/wheel/lod",collect,&r);CHECK(r.count>100);
    const char *cars[]={"civic_sir2_91","crx_sir_90","ka_01","s2000_a_99","focus_rs_02","mustang_gt_05","r34_gtr_v_gtcolor_pc_01"};
    unsigned cases=0;
    for(int mode=0;mode<2;mode++)
    for(unsigned c=0;c<sizeof cars/sizeof *cars;c++){
        car_model *stock=mode?car_model_load_parts(vol,tables,cars[c],0,"w000",NULL):car_model_load_race_parts(vol,tables,cars[c],0,6,0,"w000",NULL);CHECK(stock);
        for(unsigned i=0;i<r.count;i++){
            car_model *changed=mode?car_model_load_parts(vol,tables,cars[c],0,r.names[i],NULL):car_model_load_race_parts(vol,tables,cars[c],0,6,0,r.names[i],NULL);CHECK(changed);
            if(argc>2 && !c && !i)changed->car->triangles[0].v[0].rgb[0]+=1;
            if(!same_body(stock,changed)){fprintf(stderr,"body changed: %s rim %s\n",cars[c],r.names[i]);return 1;}
            for(int w=0;w<4;w++)CHECK(changed->wheel[w] && changed->wheel[w]->count);
            car_model_free(changed);cases++;
        }
        car_model_free(stock);
        printf("PASS %s %s: %u GT Auto rims preserve body vertices, materials and texture bytes\n",mode?"menu":"race",cars[c],r.count);
    }
    car_model_tables_close(tables);gt4_vol_close(vol);
    printf("PASS %u race/menu car/rim combinations; visual artifact coverage remains separate\n",cases);return 0;
}
