/* P: optional lamp emitters inferred from authored lens geometry.
   Lit pressed brake surfaces are also lenses: their texture/branch, not the
   unlit material kind alone, identifies them. No per-car coordinates.
   Material/normal/position classification is an approximation, not original
   light metadata or a PS2 fidelity claim. */
#ifndef MOON_CAR_LAMPS_H
#define MOON_CAR_LAMPS_H
#include "model.h"
#include <math.h>
#include <string.h>
typedef struct {float pos[3],direction[3],color[3];int rear;} moon_car_lamp;
static unsigned moon_car_lamps(const gt4_model *m,moon_car_lamp out[4]) {
    memset(out,0,4*sizeof *out);
    if(!m || !m->count) return 0;
    float low[3]={1e9f,1e9f,1e9f},high[3]={-1e9f,-1e9f,-1e9f},weight[4]={0};
    for(int i=0;i<m->count;i++) if(!m->triangles[i].attachment)
        for(int j=0;j<3;j++) for(int k=0;k<3;k++) {
            float x=m->triangles[i].v[j].p[k];low[k]=fminf(low[k],x);high[k]=fmaxf(high[k],x);
        }
    float length=high[2]-low[2],middle=(high[0]+low[0])*.5f;
    for(int i=0;i<m->count;i++) {
        const gt4_triangle *t=m->triangles+i;
        if (t->attachment || t->lamp == 1 ||
            (!t->night_lens && (t->lamp != 2 || (t->kind != 0 && t->kind != 1 && t->kind != 5))))
            continue;
        float p[3]={0},n[3]={0},rgb[3]={0};
        for(int j=0;j<3;j++)for(int k=0;k<3;k++) {p[k]+=t->v[j].p[k]/3;n[k]+=t->v[j].normal[k]/3;rgb[k]+=t->v[j].rgb[k]/3;}
        int rear=p[2]>high[2]-.22f*length;
        if(!rear && p[2]>=low[2]+.22f*length)continue;
        /* Geometry supplies direction and position; tint is restrained white
           front / red rear. Texture-free LED classification remains approximate. */
        float ab[3],ac[3],cross[3];
        for(int k=0;k<3;k++){ab[k]=t->v[1].p[k]-t->v[0].p[k];ac[k]=t->v[2].p[k]-t->v[0].p[k];}
        cross[0]=ab[1]*ac[2]-ab[2]*ac[1];cross[1]=ab[2]*ac[0]-ab[0]*ac[2];cross[2]=ab[0]*ac[1]-ab[1]*ac[0];
        float area=sqrtf(cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2]);
        if(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]<1e-8f && area>1e-8f)
            for(int k=0;k<3;k++) n[k]=cross[k]/area;
        if(fabsf(n[2])<.35f || p[1]<low[1]+.1f*(high[1]-low[1]))continue;
        int slot=rear*2+(p[0]>middle);weight[slot]+=area;
        for(int k=0;k<3;k++){out[slot].pos[k]+=p[k]*area;out[slot].direction[k]+=n[k]*area;}
        out[slot].rear=rear;
    }
    unsigned count=0;
    for(int i=0;i<4;i++)if(weight[i]>1e-8f){
        moon_car_lamp lamp=out[i];float norm=0;
        for(int k=0;k<3;k++){lamp.pos[k]/=weight[i];norm+=lamp.direction[k]*lamp.direction[k];}
        norm=sqrtf(norm);
        for(int k=0;k<3;k++)lamp.direction[k]/=fmaxf(norm,1e-6f);
        /* Face normals can point inward on double-sided lens meshes. */
        if(lamp.direction[2]*(lamp.rear?1:-1)<0)for(int k=0;k<3;k++)lamp.direction[k]=-lamp.direction[k];
        lamp.color[0]=1;lamp.color[1]=lamp.rear?.015f:.9f;lamp.color[2]=lamp.rear?.005f:.75f;
        out[count++]=lamp;
    }
    /* Curved outer lenses have outward/upward normals, not an optical axis.
       Pair-average their authored horizontal direction; a generic 1.7-degree
       downward aim is an explicit presentation parameter, never per-car data. */
    for(int rear=0;rear<2;rear++) {
        float axis[3]={0};
        for(unsigned i=0;i<count;i++)if(out[i].rear==rear){axis[0]+=out[i].direction[0];axis[2]+=out[i].direction[2];}
        float norm=sqrtf(axis[0]*axis[0]+axis[2]*axis[2]);
        if(norm>1e-6f)for(unsigned i=0;i<count;i++)if(out[i].rear==rear){
            out[i].direction[0]=axis[0]/norm;out[i].direction[1]=rear?0:-.03f;out[i].direction[2]=axis[2]/norm;
            float n=sqrtf(1+out[i].direction[1]*out[i].direction[1]);for(int k=0;k<3;k++)out[i].direction[k]/=n;
        }
    }
    return count;
}
#endif
