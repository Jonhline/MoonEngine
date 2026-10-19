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
"uniform int u_modern_lighting,u_mode,u_hdr;\n"
"vec3 modernLinear(vec3 c){return mix(c/12.92,pow(max((c+.055)/1.055,vec3(0.)),vec3(2.4)),step(vec3(.04045),c));}\n"
"vec3 modernDisplay(vec3 c){c=max(c,vec3(0.));c=clamp((c*(2.51*c+.03))/(c*(2.43*c+.59)+.14),0.,1.);return mix(c*12.92,1.055*pow(c,vec3(1./2.4))-.055,step(vec3(.0031308),c));}\n"
/* P: use the retained course daylight calibration for both surface families.
   The coefficients are artistic, not recovered original-game constants. */
"vec3 daylightDirect(){return u_sun_color*.6;}\n"
"vec3 daylightAmbient(vec3 n,float contact){return u_sky_color*.8*(.12+.88*(.5+.5*n.y))*(1.-contact*.8);}\n"
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
"   /* P: source maps retain half-intensity encoding; recover radiance before applying dielectric Fresnel, rather than relighting an already weighted overlay. */\n"
"   /* P: bound the additive legacy clearcoat to 16 percent; this layer is not a complete energy-conserving material. */\n"
"   float reflectionFresnel=.04+.12*pow(1.-clamp(dot(n,v),0.,1.),5.);\n"
"   vec3 reflected=modernLinear(clamp(environment*2.,0.,1.))*clamp(v_material_rgb,0.,1.)*clamp(u_envk.x,0.,1.)*reflectionFresnel;return u_hdr==1?reflected:modernDisplay(reflected); }\n"
" vec3 result=vec3(0.);\n"
" for(int i=0;i<4;i++){if(i>=(u_daylight>0?1:u_nlights))break;\n"
"   float ll=length(u_ldir[i]);if(ll<1e-6)continue;vec3 l=u_daylight>0?u_sun_direction:u_ldir[i]/ll;vec3 h=(l+v)/max(length(l+v),1e-6);\n"
"   float nl=max(dot(n,l),0.),nh=max(dot(n,h),0.),vh=max(dot(v,h),0.);\n"
"   float d=a2/(3.14159265*pow(nh*nh*(a2-1.)+1.,2.));\n"
"   float smith=.5/max(nl*sqrt(nv*nv*(1.-a2)+a2)+nv*sqrt(nl*nl*(1.-a2)+a2),1e-5);\n"
"   vec3 f=f0+(1.-f0)*pow(1.-vh,5.);\n"
"   vec3 diffuse=u_specular_only==1?vec3(0.):(1.-f)*base/3.14159265;vec3 spec=d*smith*f;\n"
"   float scale=u_daylight>0?1.:u_lvertex>0.?1./u_lvertex:1.;vec3 sunTint=vec3(1.);if(u_daylight==0&&i==0&&l.y>0.){float mass=1./max(l.y,.08);vec3 trans=exp(-vec3(.0428,.1,.24414)*mass);sunTint=trans/max(dot(trans,vec3(.2126,.7152,.0722)),1e-4);}\n"
"   result+=(diffuse*(u_daylight>0?daylightDirect():max(u_ldif[i],vec3(0.)))+spec*(u_daylight>0?daylightDirect():max(u_lspe[i],vec3(0.)))*u_lspec_scale)*nl*visibility*scale*3.14159265*sunTint;\n"
"   float sky=.5+.5*n.y;vec3 skyTint=mix(vec3(.85,.78,.68),vec3(.78,.88,1.),sky);\n"
/* P: daylight sky irradiance acts on diffuse albedo. The original ambient
   coefficient is a legacy lighting response, not an independent paint colour. */
"   if(u_specular_only==0)result+=(u_daylight>0?base*daylightAmbient(n,contact):modernLinear(clamp(texel,0.,1.))*max(v_material_amb,vec3(0.))*max(u_lamb[i],vec3(0.))*skyTint*scale);\n"
" }return u_hdr==1?result:modernDisplay(result);\n"
"}\n"
