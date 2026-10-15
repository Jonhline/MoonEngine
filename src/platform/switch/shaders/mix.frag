#version 460
layout(binding=0) uniform sampler2D a;
layout(binding=1) uniform sampler2D b;
layout(binding=2) uniform sampler2D c;
layout(binding=3) uniform sampler2D d;
layout(std140,binding=0) uniform Params { vec4 p[64]; mat4 bones[20]; };
layout(location=0) in vec2 uv;
layout(location=1) in vec4 tint;
layout(location=0) out vec4 result;
void main(){result=vec4(clamp(texture(a,uv).rgb*p[1].x+texture(b,uv).rgb*p[1].y+texture(c,uv).rgb*p[1].z+texture(d,uv).rgb*p[1].w,0.,1.),1.);}
