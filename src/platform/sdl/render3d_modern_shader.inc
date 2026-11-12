/* P: GGX distribution, correlated Smith visibility and Schlick Fresnel.
   Equations: Khronos glTF 2.0, Appendix B (BRDF implementation).
   The original has no PBR textures: shininess -> sqrt(2/(shine+2)) and
   specular -> dielectric F0 are explicit PC material conversions, not PS2 facts.
   Daylight environment overlays replace the retained angular weight with a
   dielectric Fresnel term; legacy overlays keep the authored weight. Source light direction/colour and authored diffuse/ambient/specular stay inputs.
   PC daylight model: Rayleigh optical depth .1 at 550 nm, wavelength^-4;
   relative RGB transmittance uses 680/550/440 nm and the light elevation.
   This is a single-scattering approximation, not measured course radiometry.
   Display compression: Narkowicz, ACES Filmic Tone Mapping Curve (2016),
   an SDR fit, not a full ACES colour-management pipeline or HDR framebuffer. */
"uniform int u_modern_lighting,u_mode,u_hdr,u_local_lights,u_measure_no_env,u_measure_ggx_samples;\n"
"vec3 modernLinear(vec3 c){return mix(c/12.92,pow(max((c+.055)/1.055,vec3(0.)),vec3(2.4)),step(vec3(.04045),c));}\n"
/* P: authored course radiance is already display-referred. Lift it into
   the scene's radiance domain before the single final filmic compression. */
"vec3 authoredSceneLinear(vec3 displayLinear){vec3 y=clamp(displayLinear,0.,1.);vec3 a=2.51-2.43*y,b=.03-.59*y;return max((-b+sqrt(b*b+.56*a*y))/(2.*a),vec3(0.));}\n"
"vec3 modernDisplay(vec3 c){c=max(c,vec3(0.));c=clamp((c*(2.51*c+.03))/(c*(2.43*c+.59)+.14),0.,1.);return mix(c*12.92,1.055*pow(c,vec3(1./2.4))-.055,step(vec3(.0031308),c));}\n"
/* P: use the retained course daylight calibration for both surface families.
   The coefficients are artistic, not recovered original-game constants. */
"float cloudSunVisibility(vec3 world);\n"
/* Direct colour already includes elevation-dependent atmospheric extinction.
   Do not apply a constant warm filter at noon. */
"vec3 daylightDirect(){return u_sun_color;}\n"
"vec3 daylightAmbient(vec3 n,float contact){float cloud=cloudSunVisibility(v_shadow_world);vec3 sky=u_sky_color*.330863143+daylightDirect()*(1.-cloud)*.5;return sky*(.12+.88*(.5+.5*n.y))*(1.-contact*.8);}\n"
/* P: integrate the GGX environment lobe in linear radiance, rather than
   multiplying a sharp sphere-map sample by an arbitrary Fresnel cap. The
   authored reflection pass supplies the coating mask; it does not identify
   metal or paint. Its smooth coat uses roughness 0.1 independently of the
   unrelated legacy highlight exponent (P approximation). Hammersley importance sampling follows Karis/Filament.
   Thirty-two samples are a bounded approximation, not a prefiltered HDR probe. */
"float environmentRadical(uint b){b=(b<<16u)|(b>>16u);b=((b&0x55555555u)<<1u)|((b&0xAAAAAAAAu)>>1u);b=((b&0x33333333u)<<2u)|((b&0xCCCCCCCCu)>>2u);b=((b&0x0F0F0F0Fu)<<4u)|((b&0xF0F0F0F0u)>>4u);b=((b&0x00FF00FFu)<<8u)|((b&0xFF00FF00u)>>8u);return float(b)*2.3283064365386963e-10;}\n"
"uniform int u_linear_environment;uniform sampler2D u_environment_linear;\n"
"vec3 environmentRadiance(vec3 l){vec3 r=vec3(dot(l,u_r),dot(l,u_u),-dot(l,u_f));r.z+=1.;float d=2.*length(r);vec2 uv=d>1e-6?vec2(.5+r.x/d,.5-r.y/d):vec2(.5);if(u_linear_environment==1)return texture(u_environment_linear,uv).rgb;vec2 at=clamp(uv*vec2(u_envsize)-.5,vec2(0.),vec2(u_envsize-1));ivec2 a=ivec2(at),b=min(a+1,u_envsize-1);vec2 f=fract(at);return mix(mix(modernLinear(clamp(texelFetch(u_envtex,a,0).rgb*2.,0.,1.)),modernLinear(clamp(texelFetch(u_envtex,ivec2(b.x,a.y),0).rgb*2.,0.,1.)),f.x),mix(modernLinear(clamp(texelFetch(u_envtex,ivec2(a.x,b.y),0).rgb*2.,0.,1.)),modernLinear(clamp(texelFetch(u_envtex,b,0).rgb*2.,0.,1.)),f.x),f.y);}\n"
"vec3 environmentGGX(vec3 n,vec3 v,float rough){if(u_envsize.x<=0||u_measure_ggx_samples<=0)return vec3(0.);float nv=max(dot(n,v),.001),a=max(rough*rough,.001),a2=a*a;vec3 axis=abs(n.z)<.999?vec3(0,0,1):vec3(1,0,0);vec3 t=normalize(cross(axis,n)),b=cross(n,t),sum=vec3(0.);uint samples=uint(max(1,u_measure_ggx_samples));for(uint i=0u;i<samples;i++){vec2 xi=vec2((float(i)+.5)/float(samples),environmentRadical(i));float phi=6.28318530718*xi.x,c=sqrt((1.-xi.y)/(1.+(a2-1.)*xi.y)),s=sqrt(max(0.,1.-c*c));vec3 h=t*(cos(phi)*s)+b*(sin(phi)*s)+n*c;float vh=max(dot(v,h),0.);vec3 l=2.*vh*h-v;float nl=max(dot(n,l),0.);if(nl>0.&&vh>0.){float nh=max(dot(n,h),.001);float vis=.5/max(nl*sqrt(nv*nv*(1.-a2)+a2)+nv*sqrt(nl*nl*(1.-a2)+a2),1e-6);float f=.04+.96*pow(1.-vh,5.);sum+=environmentRadiance(l)*(4.*vis*nl*vh/nh)*f;}}return sum/float(samples);}\n"
"vec3 modernShade(vec3 texel,float visibility,float contact,vec3 fallback,vec3 environment){\n"
" vec3 n=v_material_normal/max(length(v_material_normal),1e-6),v=-v_material_ray/max(length(v_material_ray),1e-6);float nv=max(dot(n,v),.001);\n"
" if(length(v_material_normal)<1e-6)return fallback*visibility;\n"
/* P: normalize the retained GS texture modulation white (128) before linear
   daylight shading, matching the course normalization rather than decoding
   a half-intensity colour as a dark physical albedo. Classic is unchanged. */
" vec3 albedo=clamp(texel*(u_daylight>0 && u_has_tex==1?255./128.:1.),0.,1.);vec3 base=modernLinear(albedo)*clamp(v_material_rgb,0.,1.);\n"
" vec3 f0=.04*clamp(v_material_spec,0.,1.);\n"
" float rough=clamp(sqrt(2./(max(v_material_shine,0.)+2.)),.08,1.);float variance=max(dot(dFdx(n),dFdx(n)),dot(dFdy(n),dFdy(n)));float a2=clamp(pow(rough,4.)+variance, .0001,1.);\n"
" if(u_reflect==1){if(u_daylight==0)return u_hdr==1?modernLinear(max(fallback,vec3(0.))):fallback;\n"
/* P: the authored reflection layer is the coating mask, independently of
   base-material specular (often zero on paint). Its smooth lobe must reflect
   direct solar irradiance as well as the environment. Same GGX/Smith/Schlick
   response and solar calibration as the base lobe; visibility applies once. */
"   vec3 reflected=u_measure_no_env==1?vec3(0.):environmentGGX(n,v,.1);\n"
"   if(u_local_lights==0){vec3 l=u_sun_direction;float nl=max(dot(n,l),0.);vec3 h=(l+v)/max(length(l+v),1e-6);float nh=max(dot(n,h),0.),vh=max(dot(v,h),0.);float a2=.0001;float d=a2/(3.14159265*pow(nh*nh*(a2-1.)+1.,2.));float smith=.5/max(nl*sqrt(nv*nv*(1.-a2)+a2)+nv*sqrt(nl*nl*(1.-a2)+a2),1e-6);float f=.04+.96*pow(1.-vh,5.);reflected+=daylightDirect()*(d*smith*f*nl*3.14159265)*visibility*cloudSunVisibility(v_shadow_world);}\n"
"   reflected*=clamp(v_material_rgb,0.,1.)*clamp(u_envk.x,0.,1.);return u_hdr==1?reflected:modernDisplay(reflected); }\n"
" bool solar=u_daylight>0 && u_local_lights==0;vec3 result=vec3(0.);\n"
" for(int i=0;i<4;i++){if(i>=(solar?1:u_nlights))break;\n"
"   float ll=length(u_ldir[i]);if(ll<1e-6)continue;vec3 l=solar?u_sun_direction:u_ldir[i]/ll;vec3 h=(l+v)/max(length(l+v),1e-6);\n"
"   float nl=max(dot(n,l),0.),nh=max(dot(n,h),0.),vh=max(dot(v,h),0.);\n"
"   float d=a2/(3.14159265*pow(nh*nh*(a2-1.)+1.,2.));\n"
"   float smith=.5/max(nl*sqrt(nv*nv*(1.-a2)+a2)+nv*sqrt(nl*nl*(1.-a2)+a2),1e-5);\n"
"   vec3 f=f0+(1.-f0)*pow(1.-vh,5.);\n"
"   vec3 diffuse=u_specular_only==1?vec3(0.):(1.-f)*base/3.14159265;vec3 spec=u_measure_no_spec==1?vec3(0.):d*smith*f;\n"
"   float scale=solar?1.:u_lvertex>0.?1./u_lvertex:1.;vec3 sunTint=vec3(1.);if(u_daylight==0&&u_local_lights==0&&i==0&&l.y>0.){float mass=1./max(l.y,.08);vec3 trans=exp(-vec3(.0428,.1,.24414)*mass);sunTint=trans/max(dot(trans,vec3(.2126,.7152,.0722)),1e-4);}\n"
"   result+=(diffuse*(solar?daylightDirect():max(u_ldif[i],vec3(0.)))+spec*(solar?daylightDirect():max(u_lspe[i],vec3(0.)))*u_lspec_scale)*nl*(u_local_lights==1?1.:visibility)*(solar?cloudSunVisibility(v_shadow_world):1.)*scale*3.14159265*sunTint;\n"
"   float sky=.5+.5*n.y;vec3 skyTint=mix(vec3(.85,.78,.68),vec3(.78,.88,1.),sky);\n"
/* P: daylight sky irradiance acts on diffuse albedo. The original ambient
   coefficient is a legacy lighting response, not an independent paint colour. */
"   if(u_specular_only==0)result+=(solar?base*daylightAmbient(n,contact):modernLinear(clamp(texel,0.,1.))*max(v_material_amb,vec3(0.))*max(u_lamb[i],vec3(0.))*(u_local_lights==1?vec3(1.):skyTint)*scale);\n"
" }return u_hdr==1?result:modernDisplay(result);\n"
"}\n"
