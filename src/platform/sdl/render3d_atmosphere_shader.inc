/* P: single-scattering daytime atmosphere, not a texture recolour.
   Rayleigh/Henyey-Greenstein phases: pbr-book.org, Volume Scattering.
   RGB sea-level Rayleigh coefficient and density scale heights follow the
   Earth example of Bruneton's atmosphere model (units kilometres).
   Eight view samples/four solar samples deliberately approximate integration;
   no multiple scattering, ozone, geolocation or weather forecast.
   The optional night extension is an artistic star/moon model. */
"uniform int u_night_sky;\n"
"float nightHash(vec3 p){uvec3 q=uvec3(ivec3(p));uint h=q.x*1597334677u^q.y*3812015801u^q.z*2798796415u;h^=h>>16;h*=2246822519u;h^=h>>13;return float(h&16777215u)/16777216.;}\n"
/* P: a bounded low-cloud slab, not authored GT4 weather. Beer-Lambert
   extinction and single-scattering ordering follow PBRT volume transport;
   noise/profile and optical depth are artistic approximations. */
"uniform float u_sky_seconds;uniform vec3 u_cam;\n"
"float cloudNoise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(mix(nightHash(i),nightHash(i+vec3(1,0,0)),f.x),mix(nightHash(i+vec3(0,1,0)),nightHash(i+vec3(1,1,0)),f.x),f.y),mix(mix(nightHash(i+vec3(0,0,1)),nightHash(i+vec3(1,0,1)),f.x),mix(nightHash(i+vec3(0,1,1)),nightHash(i+vec3(1,1,1)),f.x),f.y),f.z); }\n"
"float cloudDensity(vec3 p){vec3 wind=vec3(7.,0.,1.5)*mod(u_sky_seconds,86400.);vec3 q=(p+wind)*.00055;float n=0.,weight=.5;for(int i=0;i<4;i++){n+=cloudNoise(q)*weight;q=q*2.03+vec3(19.1,3.7,9.2);weight*=.5;}float h=(p.y-1500.)/700.;return smoothstep(.56,.72,n)*smoothstep(0.,.18,h)*(1.-smoothstep(.65,1.,h));}\n"
"float cloudSunReference(vec3 p){if(u_night_sky==0 || u_sun_direction.y<=0.)return 1.;float t=(1850.-p.y)/max(u_sun_direction.y,.08);if(t<=0.)return 1.;return exp(-cloudDensity(p+u_sun_direction*t)*2.4/max(u_sun_direction.y,.15));}\n"
"vec4 cloudVolume(vec3 ray){if(u_night_sky==0 || ray.y<=.015)return vec4(0.,0.,0.,1.);float start=max((1500.-u_cam.y)/ray.y,0.),finish=max((2200.-u_cam.y)/ray.y,0.);if(finish<=start)return vec4(0.,0.,0.,1.);float stepLength=min(finish-start,40000.)/32.,transmission=1.;vec3 light=vec3(0.);float daylight=smoothstep(-.12,.08,u_sun_direction.y);\n"
" for(int i=0;i<32;i++){vec3 p=u_cam+ray*(start+(float(i)+.5)*stepLength);float density=cloudDensity(p),opacity=1.-exp(-density*.0034*stepLength);float shadow=0.;for(int j=0;j<3;j++)shadow+=cloudDensity(p+u_sun_direction*(float(j)+.5)*220.);float solar=exp(-shadow*.0034*220.);float mu=dot(ray,u_sun_direction),g=.55;float phase=(1.-g*g)/pow(max(1.+g*g-2.*g*mu,.001),1.5);vec3 radiance=u_sky_color*.35+u_sun_color*solar*(.45+.12*phase)*daylight;light+=transmission*opacity*radiance;transmission*=1.-opacity;}float horizon=smoothstep(.02,.15,ray.y);return vec4(light*horizon,mix(1.,transmission,horizon)); }\n"
#ifndef MOON_CLOUD_BAKE
"uniform int u_cloud_cache;uniform sampler2D u_cloud_volume,u_cloud_shadow;uniform vec2 u_cloud_origin;\n"
"float cloudSunVisibility(vec3 p){if(u_cloud_cache==1){vec2 uv=(p.xz-u_cloud_origin)/8192.+.5;if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return 1.;return texture(u_cloud_shadow,uv).r;}return cloudSunReference(p);}\n"
"vec4 cloudSample(vec3 ray){if(u_cloud_cache==1){if(ray.y<=.015)return vec4(0,0,0,1);return texture(u_cloud_volume,vec2(atan(ray.z,ray.x)/6.28318530718+.5,sqrt(clamp(ray.y,0.,1.))));}return cloudVolume(ray);}\n"
#else
"float cloudSunVisibility(vec3 p){return cloudSunReference(p);}\n"
"vec4 cloudSample(vec3 ray){return cloudVolume(ray);}\n"
#endif
"vec3 cloudSky(vec3 ray,vec3 background){vec4 cloud=cloudSample(ray);return background*cloud.a+cloud.rgb;}\n"
"vec3 nightSky(vec3 ray){\n"
" vec3 pole=vec3(0.,.573576436,.819152044);float c=clamp(u_sun_direction.y/.819152044,-1.,1.),s=-u_sun_direction.x;\n"
" vec3 celestial=ray*c-cross(pole,ray)*s+pole*dot(pole,ray)*(1.-c);\n"
" vec3 cell=floor(celestial*768.);float above=step(0.,ray.y);float star=step(.99,nightHash(cell))*pow(max(ray.y,0.),.35)*above;\n"
" vec3 moonDirection=-u_sun_direction;float moon=dot(ray,moonDirection);float disc=smoothstep(cos(.009),cos(.008),moon)*above;\n"
" vec3 transmission=exp(-vec3(.0428,.1,.24414)/max(moonDirection.y,.08));\n"
" return vec3(.0006,.0009,.002)+vec3(.22,.25,.3)*star+vec3(.45,.48,.55)*transmission*disc;}\n"
"vec2 atmosphereBoundary(vec3 p,vec3 ray,float radius){float b=dot(p,ray),h=b*b-dot(p,p)+radius*radius;if(h<0.)return vec2(-1.);h=sqrt(h);return vec2(-b-h,-b+h);}\n"
"vec3 clearAtmosphereSky(vec3 ray){if(u_night_sky==1 && u_sun_direction.y<=-.12)return nightSky(normalize(ray));\n"
" const float earth=6360.,top=6460.;vec3 origin=vec3(0.,earth+.002,0.);\n"
" vec3 nightRay=normalize(ray);ray=normalize(vec3(ray.x,max(ray.y,.001),ray.z));vec2 hit=atmosphereBoundary(origin,ray,top);\n"
" float stepLength=hit.y/8.;vec2 optical=vec2(0.);vec3 totalR=vec3(0.),totalM=vec3(0.);\n"
" const vec3 betaR=vec3(.005802,.013558,.0331);const vec3 betaM=vec3(.003996);\n"
" for(int i=0;i<8;i++){vec3 p=origin+ray*((float(i)+.5)*stepLength);float height=max(length(p)-earth,0.);\n"
" vec2 density=exp(-vec2(height/8.,height/1.2))*stepLength;optical+=density*.5;\n"
" float sunLength=atmosphereBoundary(p,u_sun_direction,top).y/4.;vec2 solar=vec2(0.);\n"
" for(int j=0;j<4;j++){vec3 q=p+u_sun_direction*((float(j)+.5)*sunLength);float h=max(length(q)-earth,0.);solar+=exp(-vec2(h/8.,h/1.2))*sunLength;}\n"
" vec3 transmission=exp(-betaR*(optical.x+solar.x)-betaM*(optical.y+solar.y)*1.1);\n"
" totalR+=density.x*transmission;totalM+=density.y*transmission;optical+=density*.5;}\n"
" float mu=dot(ray,u_sun_direction),phaseR=3.*(1.+mu*mu)/(16.*3.14159265);\n"
" const float g=.76;float phaseM=(1.-g*g)/(4.*3.14159265*pow(max(1.+g*g-2.*g*mu,1e-5),1.5));\n"
" vec3 sky=(totalR*betaR*phaseR+totalM*betaM*phaseM)*20.;\n"
" float disc=smoothstep(cos(.0052),cos(.0042),mu);\n"
" sky+=exp(-betaR*8./max(u_sun_direction.y,.08)-betaM*1.2/max(u_sun_direction.y,.08))*disc*4.;\n"
" if(u_night_sky==1){float day=smoothstep(-.12,.08,u_sun_direction.y);sky=mix(nightSky(nightRay),sky,day);}\n"
" return max(sky,vec3(0.));}\n"
"vec3 atmosphereSky(vec3 ray){return cloudSky(normalize(ray),clearAtmosphereSky(ray));}\n"
#ifndef MOON_CLOUD_BAKE
"vec3 sceneSky(vec3 ray){if(u_cloud_cache==1 && ray.y>.015 && u_sun_direction.y>-.12){if(dot(ray,u_sun_direction)>.999){vec4 c=cloudVolume(ray);return clearAtmosphereSky(ray)*c.a+c.rgb;}return texture(u_cloud_volume,vec2(atan(ray.z,ray.x)/6.28318530718+.5,sqrt(clamp(ray.y,0.,1.)))).rgb;}return atmosphereSky(ray);}\n"
#endif
