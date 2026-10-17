#version 460
layout(binding=0) uniform sampler2D source;
layout(location=0) out vec4 result;
void main(){result=vec4(texelFetch(source,ivec2(gl_FragCoord.xy),0).a);}
