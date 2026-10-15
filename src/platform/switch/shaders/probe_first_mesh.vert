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
layout(location=4) out float fog;
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
 float nearz=p[8].z;
 gl_Position=vec4(x*p[8].x,y*p[8].y,(z-nearz)*20000./(20000.-nearz),z);
 gl_Position.xy+=p[15].xy*z;
 vec3 wn=normalize(rot*n);if(p[7].w>0.)wn.y=-wn.y;
 vec3 lit=diffuse,sp=vec3(0.);
 if(p[9].x==1.)lit=diffuse*p[9].y;
 else if(p[9].x==2.)lit=diffuse+p[16].xyz;
 else if(p[9].z==0.){
  lit=vec3(0.);int count=int(p[10].w);
  if(count==0){lit=ambient*.196+diffuse*max(0.,dot(wn,normalize(vec3(.4,.8,-.5))));}
  for(int i=0;i<count;i++){
   vec3 l=p[24+i].xyz;float len=length(l);float d=len>1e-6?max(0.,dot(wn,l/len)):0.;
   vec3 h=len>1e-6?l/len-p[7].xyz:vec3(0.);float hn=length(h);
   float s=d>0. && hn>1e-6?pow(max(0.,dot(wn,h/hn)),shine_alpha.x):0.;
   lit+=ambient*p[28+i].xyz+diffuse*p[32+i].xyz*d;sp+=specular*p[36+i].xyz*s;
  }
  if(p[14].w>0.){lit=clamp(lit+sp,vec3(0.),vec3(p[14].w));sp=vec3(0.);}
 }
 if(p[9].w>0.){lit=diffuse;sp=vec3(0.);}
 if(p[14].y>0.){lit=sp;sp=vec3(0.);}
 if(p[17].w>0.)lit=lit*p[17].xyz+p[18].xyz;
 vec3 nn=vec3(dot(wn,p[5].xyz),dot(wn,p[6].xyz),-dot(wn,p[7].xyz));
 vec3 incident=normalize(vec3(x,y,-max(z,1e-6)));vec3 r=reflect(incident,nn);r.z+=1.;
 envuv=length(r)>1e-6?vec2(.5+r.x/(2.*length(r)),.5-r.y/(2.*length(r))):vec2(.5);
 if(p[9].w>0.)lit=diffuse*(p[19].x+p[19].y*min(dot(rel,wn),0.)/max(length(rel),1e-6));
 tint=vec4(lit,shine_alpha.y*(p[9].x==2.?p[16].w:1.));shine=sp;uv=coord;
 fog=p[20].w>0.?clamp(p[20].y+(1.-p[20].y)*min(max(z,1e-6),p[20].x)/max(z,1e-6),0.,1.):1.;
}
