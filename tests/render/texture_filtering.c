/* P: an elongated texture footprint must preserve minor-axis detail when opted in. */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "render3d.h"
#include "video_gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) {fprintf(stderr,"texture filtering %d: %s\n",__LINE__,#x);return 1;} } while(0)
static int draw(r3d_mesh *mesh, int mode, uint8_t *pixels)
{
    r3d_texture_filtering(mode);
    r3d_view v={0};v.rot[0]=v.rot[4]=v.rot[8]=1;
    v.r[0]=v.u[1]=v.f[2]=1;v.focal=320;v.focal_y=224;v.unlit=1;
    if(!r3d_begin_target(128)) return 0;
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    r3d_begin(0,0,640,448,NULL);r3d_draw(mesh,&v);r3d_end();r3d_end_target(pixels);return 1;
}
static int contrast(const uint8_t *p)
{
    int lo=255,hi=0;
    for(int y=48;y<80;y++)for(int x=24;x<104;x++) {
        int value=p[(y*128+x)*4];if(value<lo)lo=value;if(value>hi)hi=value;
    }
    return hi-lo;
}
int main(int argc,char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    tex1_image image={0};image.width=image.height=image.tex_w=image.tex_h=256;
    image.minify=5;image.magnify=1;image.mip_count=6;
    image.rgba=malloc(256*256*4);CHECK(image.rgba);
    for(int y=0;y<256;y++)for(int x=0;x<256;x++) {
        uint8_t *p=image.rgba+(y*256+x)*4;p[0]=p[1]=p[2]=((x/16)&1)?255:0;p[3]=128;
    }
    for(int level=1;level<=6;level++) {
        int size=256>>level,previous=size*2;
        const uint8_t *src=level==1?image.rgba:image.mip_rgba[level-2];
        uint8_t *dst=image.mip_rgba[level-1]=malloc((size_t)size*size*4);CHECK(dst);
        for(int y=0;y<size;y++)for(int x=0;x<size;x++)for(int c=0;c<4;c++)
            dst[(y*size+x)*4+c]=(src[((2*y)*previous+2*x)*4+c]+src[((2*y)*previous+2*x+1)*4+c]+
                src[((2*y+1)*previous+2*x)*4+c]+src[((2*y+1)*previous+2*x+1)*4+c]+2)/4;
    }
    gt4_triangle tris[2]={0};
    const float positions[6][3]={{-3,-3,3},{3,-3,3},{3,3,3},{-3,-3,3},{3,3,3},{-3,3,3}};
    for(int i=0;i<2;i++) {
        tris[i].kind=0;tris[i].texture=0;tris[i].state.depth_func=1;tris[i].state.depth_write=1;
        for(int j=0;j<3;j++) {
            gt4_vertex *v=&tris[i].v[j];memcpy(v->p,positions[i*3+j],12);
            v->normal[2]=-1;v->alpha=1;v->rgb[0]=v->rgb[1]=v->rgb[2]=1;
            v->uv[0]=(v->p[0]+3)/6;v->uv[1]=(v->p[1]+3)/6*64;
        }
    }
    gt4_model m={0};m.triangles=tris;m.count=2;m.textures=&image;m.ntextures=1;
    r3d_mesh *mesh=r3d_mesh_create(&m);CHECK(mesh);
    uint8_t original[128*128*4],tri[128*128*4],aniso[128*128*4],restored[128*128*4];
    CHECK(draw(mesh,0,original));CHECK(draw(mesh,1,tri));
    CHECK(draw(mesh,argc>1&&!strcmp(argv[1],"--negative")?0:4,aniso));
    CHECK(draw(mesh,0,restored));
    printf("footprint contrast: original=%d trilinear=%d anisotropic16=%d\n",contrast(original),contrast(tri),contrast(aniso));
    CHECK(!memcmp(original,restored,sizeof original));
    CHECK(contrast(tri)<=2 && contrast(aniso)>contrast(tri)+20);
    r3d_mesh_destroy(mesh);
    image.wrap_u = 2; image.min_u = 0; image.max_u = 255;
    mesh = r3d_mesh_create(&m); CHECK(mesh);
    CHECK(draw(mesh,0,original)); CHECK(draw(mesh,4,restored));
    CHECK(!memcmp(original,restored,sizeof original));
    r3d_mesh_destroy(mesh);free(image.rgba);for(int i=0;i<6;i++)free(image.mip_rgba[i]);
    plat_video_shutdown();puts("PASS distant detail and byte-exact Original restoration");return 0;
}
