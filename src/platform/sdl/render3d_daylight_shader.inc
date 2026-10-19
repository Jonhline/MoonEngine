/* P: directional relighting of legacy baked surfaces. The diffuse texture
   and vertex colours are retained; old baked shadows cannot be removed. */

"vec3 daylightShade(vec3 src,vec3 texel,float visibility,float contact){\n"
" vec3 ray=normalize(v_material_ray);float distance=length(v_material_ray);\n"
" float sun=dot(ray,u_sun_direction);\n"
" if(u_daylight>=2){vec3 c=atmosphereSky(ray);return u_hdr==1?c:modernDisplay(c)*(u_daylight==3?.5:1.); }\n"
" vec3 geometric=cross(dFdx(v_shadow_world),dFdy(v_shadow_world));\n"
" vec3 normal=length(v_material_normal)>1e-5?normalize(v_material_normal):geometric/max(length(geometric),1e-6);\n"
" if(dot(normal,-ray)<0.)normal=-normal;\n"
" float nl=max(dot(normal,u_sun_direction),0.);\n"
" if(u_mode==1){vec3 base=modernLinear(clamp(src,0.,1.));src=base*(daylightAmbient(normal,contact)+daylightDirect()*nl*visibility);if(u_hdr==0)src=modernDisplay(src);}else if(u_hdr==1 && u_modern_lighting==0)src=modernLinear(src);\n"
" float haze=1.-exp(-distance*.00012);vec3 scatter=u_haze_color*(.45+.55*pow(max(sun,0.),16.));\n"
" return mix(src,scatter,haze*.55);}\n"
