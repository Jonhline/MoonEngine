/* P: single-scattering daytime atmosphere, not a texture recolour.
   Rayleigh/Henyey-Greenstein phases: pbr-book.org, Volume Scattering.
   RGB sea-level Rayleigh coefficient and density scale heights follow the
   Earth example of Bruneton's atmosphere model (units kilometres).
   Eight view samples/four solar samples deliberately approximate integration;
   no multiple scattering, ozone, geolocation, weather forecast or night. */
"vec2 atmosphereBoundary(vec3 p,vec3 ray,float radius){float b=dot(p,ray),h=b*b-dot(p,p)+radius*radius;if(h<0.)return vec2(-1.);h=sqrt(h);return vec2(-b-h,-b+h);}\n"
"vec3 atmosphereSky(vec3 ray){\n"
" const float earth=6360.,top=6460.;vec3 origin=vec3(0.,earth+.002,0.);\n"
" ray=normalize(vec3(ray.x,max(ray.y,.001),ray.z));vec2 hit=atmosphereBoundary(origin,ray,top);\n"
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
" return max(sky,vec3(0.));}\n"
