/* P: directional relighting of legacy baked surfaces. The diffuse texture
   and vertex colours are retained; old baked shadows cannot be removed. */

/* P: the baked course is already lit. Modulate it relative to this daytime
   model's neutral noon, rather than multiplying a second absolute ambient
   response into its existing shadows. The 35-degree reference is the same
   artistic latitude as daylight.h, not recovered PS2 lighting metadata. */
"uniform int u_authored_environment;\n"
"vec3 bakedReferenceIrradiance(vec3 normal){\n"
" if(u_authored_environment==1)return u_sky_color*.330863143*(.12+.88*(.5+.5*normal.y))+daylightDirect()*max(dot(normal,u_sun_direction),0.);\n"
" const float noonY=.819152044;vec3 noonSun=vec3(0.,noonY,-.573576436);\n"
" vec3 transmission=exp(-vec3(.0428,.1,.24414)/noonY);\n"
" vec3 direct=transmission/dot(transmission,vec3(.2126,.7152,.0722));\n"
" vec3 sky=vec3(.65,.78,1.)*(.65+.35*noonY);\n"
" return sky*.330863143*(.12+.88*(.5+.5*normal.y))+direct*max(dot(normal,noonSun),0.);\n"
"}\n"
"vec3 daylightShade(vec3 src,vec3 texel,float visibility,float contact){\n"
" vec3 ray=normalize(v_material_ray);float distance=length(v_material_ray);\n"
" float sun=dot(ray,u_sun_direction);\n"
" if(u_daylight>=2){vec3 c=sceneSky(ray);return u_hdr==1?c:modernDisplay(c)*(u_daylight==3?.5:1.); }\n"
" vec3 geometric=cross(dFdx(v_shadow_world),dFdy(v_shadow_world));\n"
" vec3 normal=length(v_material_normal)>1e-5?normalize(v_material_normal):geometric/max(length(geometric),1e-6);\n"
" if(dot(normal,-ray)<0.)normal=-normal;\n"
" float nl=max(dot(normal,u_sun_direction),0.);\n"
" if(u_mode==1){vec3 base=modernLinear(clamp(src,0.,1.));if(u_authored_environment==1)base=authoredSceneLinear(base);src=base*(daylightAmbient(normal,contact)+daylightDirect()*nl*visibility*cloudSunVisibility(v_shadow_world))/max(bakedReferenceIrradiance(normal),vec3(1e-6));if(u_hdr==0)src=modernDisplay(src);}else if(u_hdr==1 && u_modern_lighting==0)src=modernLinear(src);\n"
" float haze=1.-exp(-distance*.00012);vec3 scatter=u_haze_color*(.45+.55*pow(max(sun,0.),16.));\n"
" return mix(src,scatter,haze*.55);}\n"
