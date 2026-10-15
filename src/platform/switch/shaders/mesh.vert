#version 460
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 coord;
layout(location=3) in vec3 diffuse;
layout(location=4) in vec3 ambient;
layout(location=5) in vec3 specular;
layout(location=6) in vec2 shine_alpha;
layout(location=7) in vec4 bone_id;
layout(location=8) in vec4 weight;
layout(std140,binding=0) uniform Params { vec4 p[64]; mat4 bones[20]; };
layout(location=0) out vec2 uv;
layout(location=1) out vec4 tint;
layout(location=2) out vec3 shine;
layout(location=3) out vec2 envuv;
layout(location=4) noperspective out float fog;
layout(location=5) out float floor_y;
void main(){
 vec3 pos=position,n=normal;
 if(p[14].x>0. && dot(weight,vec4(1.))>0.){
  mat4 b=bones[int(bone_id.x)]*weight.x+bones[int(bone_id.y)]*weight.y+bones[int(bone_id.z)]*weight.z+bones[int(bone_id.w)]*weight.w;
  pos=(b*vec4(pos,1.)).xyz;n=mat3(b)*n;
 }
 mat3 rot=transpose(mat3(p[0].xyz,p[1].xyz,p[2].xyz));
 vec3 world=rot*pos+p[3].xyz;floor_y=world.y;
 if(p[7].w>0.)world.y=-world.y;
 vec3 rel=world-p[4].xyz;
 float x=dot(rel,p[5].xyz),y=dot(rel,p[6].xyz),z=dot(rel,p[7].xyz);
 float nearz=p[8].z,farz=p[8].w>0.?p[8].w:20000.;
 gl_Position=vec4(x*p[8].x,y*p[8].y,(z-nearz)*farz/(farz-nearz),z);
 gl_Position.xy+=p[15].xy*z;
 vec3 wn=normalize(rot*n);if(p[7].w>0.)wn.y=-wn.y;
 vec3 lit=diffuse,sp=vec3(0.);
 if(p[9].x==1.)lit=p[14].y>0.?vec3(0.):diffuse*p[9].y;
 else if(p[9].x==2.)lit=diffuse+p[16].xyz;
 else if(p[9].z==0.){
  lit=vec3(0.);int count=int(p[10].w);
  // Same fallback lights as render3d.c when no scene lights are supplied.
  if(count==0){
   const vec3 L[3]=vec3[3](vec3(.57735,.57735,.57735),vec3(0.,.9759,-.2182),vec3(-1.,0.,0.));
   const vec3 D=vec3(1.,60./255.,216./255.),S=vec3(239./255.,108./255.,198./255.);
   float ds=0.,ss=0.;for(int i=0;i<3;i++){float d=dot(wn,L[i]);ds+=D[i]*max(0.,d);
    vec3 h=L[i]-p[7].xyz;float hn=length(h);if(d>0. && hn>1e-6)ss+=S[i]*pow(max(0.,dot(wn,h)/hn),shine_alpha.x);}
   lit=ambient*(50./255.)+diffuse*ds;sp=specular*ss*.18;
  }
  for(int i=0;i<count;i++){
   vec3 l=p[24+i].xyz;float len=length(l);float d=len>1e-6?max(0.,dot(wn,l/len)):0.;
   vec3 h=len>1e-6?l/len-p[7].xyz:vec3(0.);float hn=length(h);
   float s=d>0. && hn>1e-6?pow(max(0.,dot(wn,h/hn)),shine_alpha.x):0.;
   // VU1 0x23/0x52/0x83 (0049EA18): fourth/eighth/sixteenth power.
   if(p[14].w>0.){float e=hn>1e-6?max(0.,dot(wn,h/hn)):0.;e*=e;e*=e;
    if(shine_alpha.x>=6.)e*=e;if(shine_alpha.x>=12.)e*=e;s=e;}
   lit+=ambient*p[28+i].xyz+diffuse*p[32+i].xyz*d;sp+=specular*p[36+i].xyz*s;
  }
  if(count>0){if(p[14].w>0.){lit=clamp(lit+sp,vec3(0.),vec3(p[14].w));sp=vec3(0.);}else sp*=p[10].z;}
 }
 if(p[9].x==0. && p[9].w>0.){lit=diffuse;sp=vec3(0.);}
 if(p[9].x==0. && p[14].y>0.){lit=sp;sp=vec3(0.);}
 if(p[9].x!=0. && p[17].w>0.)lit=lit*p[17].xyz+p[18].xyz;
 vec3 nn=vec3(dot(wn,p[5].xyz),dot(wn,p[6].xyz),-dot(wn,p[7].xyz));
 vec3 incident=normalize(vec3(x,y,-max(z,1e-6)));vec3 r=reflect(incident,nn);r.z+=1.;
 envuv=length(r)>1e-6?vec2(.5+r.x/(2.*length(r)),.5-r.y/(2.*length(r))):vec2(.5);
 if(p[9].x==0. && p[9].w>0.){lit=diffuse*(p[19].x+p[19].y*min(dot(rel,wn),0.)/max(length(rel),1e-6));sp=vec3(0.);}
 // The native GPU comparison isolated the 36fe696 vertex program:
 // it emitted alpha=1 for .25/.5/.75; 7715ae8 preserved all three.
 // Keep the same shadow clamp as render3d.c, trying an arithmetic merge
 // instead of a branch/phi for the packed colour's alpha lane. The console
 // boot checks all three fractional values before testing material blending.
 float shadow_alpha=min(shine_alpha.y*p[16].w,1.);
 float opacity=mix(shine_alpha.y,shadow_alpha,float(p[9].x==2.));
 tint=vec4(lit,opacity);shine=sp;uv=coord;
 float m=clamp(p[20].y*255.,0.,255.),fz=max(z,1e-6);
 fog=p[20].w>0.?floor(min(255.,m+.1+(255.-m)*min(fz,max(p[20].x,0.))/fz))/255.:1.;
}
