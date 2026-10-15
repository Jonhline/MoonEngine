#version 460
layout(location=0) in vec2 position;
layout(location=1) in vec2 coord;
layout(location=2) in vec4 color;
layout(std140,binding=0) uniform Params { vec4 p[64]; mat4 bones[20]; };
layout(location=0) out vec2 uv;
layout(location=1) out vec4 tint;
void main(){gl_Position=vec4(position,0.,1.);uv=coord;tint=color;}
