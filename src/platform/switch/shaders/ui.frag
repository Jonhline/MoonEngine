#version 460
layout(binding=0) uniform sampler2D image;
layout(binding=1) uniform sampler2D palette;
layout(std140,binding=0) uniform Params { vec4 p[64]; mat4 bones[20]; };
layout(location=0) in vec2 uv;
layout(location=1) in vec4 tint;
layout(location=0) out vec4 result;
void main(){vec4 c=texture(image,uv);
 if(p[0].x==2.){
  ivec3 at=ivec3(floor(c.rgb*255.+.5));
  int r=int(texelFetch(palette,ivec2(at.r,0),0).r*255.+.5);
  int g=int(texelFetch(palette,ivec2(at.g,0),0).g*255.+.5);
  int b=int(texelFetch(palette,ivec2(at.b,0),0).b*255.+.5);
  ivec3 rgb=(r*ivec3(p[40].xyz))/128+(g*ivec3(p[41].xyz))/128+(b*ivec3(p[42].xyz))/128;
  c=vec4(vec3(min(rgb,ivec3(255)))/255.,1.);
 }
if(p[0].x==1.)c=vec4(1.,1.,1.,c.r);result=c*tint;result.a*=p[0].y;}
