#include "vk_blit_clip.h"
#include <cstdio>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"clip line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char**) {
    int a=0,b=100,c=0,d=100;
    CHECK(moon_vk_clip_axis(a,b,100,c,d,100));CHECK(a==0 && b==100 && c==0 && d==100);
    a=-132;b=1212;c=0;d=1344;
    CHECK(moon_vk_clip_axis(a,b,1080,c,d,1344));
    if(argc>1) {a=-132;b=1212;}
    CHECK(a==0 && b==1080 && c==132 && d==1212);
    a=0;b=100;c=-20;d=180;
    CHECK(moon_vk_clip_axis(a,b,100,c,d,100));CHECK(a==10 && b==60 && c==0 && d==100);
    a=100;b=0;c=-20;d=180;
    CHECK(moon_vk_clip_axis(a,b,100,c,d,100));CHECK(a==90 && b==40 && c==0 && d==100);
    a=-200;b=-100;c=0;d=100;CHECK(!moon_vk_clip_axis(a,b,100,c,d,100));
    a=0;b=0;c=0;d=100;CHECK(!moon_vk_clip_axis(a,b,100,c,d,100));
    return 0;
}
