#version 460
layout(binding=0) uniform sampler2D image;
layout(binding=1) uniform sampler2D environment;
layout(std140,binding=0) uniform Params { vec4 p[64]; mat4 bones[20]; };
layout(location=0) in vec2 uv;
layout(location=1) in vec4 tint;
layout(location=2) in vec3 shine;
layout(location=3) in vec2 envuv;
layout(location=4) in float fog;
layout(location=5) in float floor_y;
layout(location=0) out vec4 result;
void main(){
 if(p[7].w>0. && floor_y<0.)discard;
 vec4 texel=p[10].x>0.?texture(image,uv):vec4(1.);
 float alpha=tint.a;
 if(p[11].x>0.){float ta=p[11].y>0.?texel.a*255./128.:1.;alpha=p[11].z==0.?alpha*ta:p[11].z==2.?alpha+ta:ta;}
 alpha=clamp(alpha,0.,1.);
 if(p[12].x>0.){
  float a=floor(alpha*128.+.5),ref=p[12].z;int f=int(p[12].y);
  bool pass=f==1 || (f==2 && a<ref)||(f==3 && a<=ref)||(f==4 && a==ref)||(f==5 && a>=ref)||(f==6 && a>ref)||(f==7 && a!=ref);
  if(!pass)discard;
 }
 vec3 rgb=p[9].w>0.?texture(environment,clamp(envuv,0.,1.)).rgb*tint.rgb:min(vec3(1.),texel.rgb*tint.rgb*(p[10].x>0.?1.:p[10].y)+shine);
 rgb=mix(p[21].xyz,rgb,fog);
 if(p[23].w>0.){float g=dot(rgb,vec3(.299,.587,.114));vec3 graded=(g+(rgb-g)*p[22].w)*p[22].xyz+p[23].x;rgb=mix(rgb,graded,p[23].w);}
 result=vec4(rgb,alpha*p[13].x);
}
