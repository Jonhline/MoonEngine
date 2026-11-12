/* GL framebuffer blits clip both rectangles; Vulkan transfer regions must be in bounds. */
#ifndef MOON_VK_BLIT_CLIP_H
#define MOON_VK_BLIT_CLIP_H
#include <algorithm>
#include <cmath>
static inline bool moon_vk_clip_axis(int &s0,int &s1,int slimit,int &d0,int &d1,int dlimit)
{
    if(s0==s1 || d0==d1 || slimit<=0 || dlimit<=0)return false;
    double lo=0.,hi=1.;
    auto clip=[&](int a,int b,int limit) {
        double x=(0.-a)/(double(b)-a),y=(double(limit)-a)/(double(b)-a);
        lo=std::max(lo,std::min(x,y));hi=std::min(hi,std::max(x,y));
    };
    clip(s0,s1,slimit);clip(d0,d1,dlimit);
    if(lo>=hi)return false;
    int sa=s0,da=d0,ds=s1-s0,dd=d1-d0;
    s0=std::clamp((int)std::lround(sa+lo*ds),0,slimit);
    s1=std::clamp((int)std::lround(sa+hi*ds),0,slimit);
    d0=std::clamp((int)std::lround(da+lo*dd),0,dlimit);
    d1=std::clamp((int)std::lround(da+hi*dd),0,dlimit);
    return s0!=s1 && d0!=d1;
}
#endif
