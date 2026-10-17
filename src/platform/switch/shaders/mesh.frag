#version 460
layout(binding=0) uniform sampler2D image;
layout(binding=1) uniform sampler2D environment;
layout(binding=2) uniform sampler2D destination;
layout(std140,binding=0) uniform Params { vec4 p[64]; mat4 bones[20]; };
layout(location=0) in vec2 uv;
layout(location=1) in vec4 tint;
layout(location=2) in vec3 shine;
layout(location=3) in vec2 envuv;
layout(location=4) noperspective in float fog;
layout(location=5) in float floor_y;
layout(location=0) out vec4 result;
// Same TEX0/CLAMP boundary as render3d.c; region modes apply to every tap.
int address(int at,int size,int mode,int mn,int mx){
 if(mode==0)return at-size*int(floor(float(at)/float(size)));
 if(mode==3)at=(at&mn)|mx;else if(mode==2)at=clamp(at,mn,mx);
 return clamp(at,0,size-1);
}
vec4 material_texel(){
 ivec2 size=ivec2(p[43].zw),wrap=ivec2(p[44].xy);ivec4 region=ivec4(p[45]);
 vec2 pos=uv*p[43].xy-.5;ivec2 q=ivec2(floor(pos));vec2 f=fract(pos);
 if(p[44].z>0.)q=ivec2(floor(uv*p[43].xy));
 int x=address(q.x,size.x,wrap.x,region.x,region.y),y=address(q.y,size.y,wrap.y,region.z,region.w);
 if(p[44].z>0.)return texelFetch(image,ivec2(x,y),0);
 if(wrap.x<2 && wrap.y<2)return texture(image,uv*p[43].xy/vec2(size));
 int nx=address(q.x+1,size.x,wrap.x,region.x,region.y),ny=address(q.y+1,size.y,wrap.y,region.z,region.w);
 return mix(mix(texelFetch(image,ivec2(x,y),0),texelFetch(image,ivec2(nx,y),0),f.x),
            mix(texelFetch(image,ivec2(x,ny),0),texelFetch(image,ivec2(nx,ny),0),f.x),f.y);
}
void main(){
 if(p[7].w>0. && floor_y<0.)discard;
 vec4 texel=p[10].x>0.?material_texel():vec4(1.);
 float alpha=tint.a;
 if(p[10].x>0. && p[11].x>0.){float ta=p[11].y>0.?texel.a*255./128.:1.;alpha=p[11].z==0.?alpha*ta:p[11].z==2.?alpha+ta:ta;}
 // The GS coefficient can exceed unity before ATST (128 is unity).
 alpha=floor(clamp(alpha*128.,0.,255.)+.5)/128.;
 if(p[12].x>0.){
  float a=floor(alpha*128.+.5),ref=p[12].z;int f=int(p[12].y);
  bool pass=f==1 || (f==2 && a<ref)||(f==3 && a<=ref)||(f==4 && a==ref)||(f==5 && a>=ref)||(f==6 && a>ref)||(f==7 && a!=ref);
  if(p[46].z==2.?pass:!pass)discard;
 }
 if(p[46].x>0.){vec4 saved=texelFetch(destination,ivec2(gl_FragCoord.xy),0);bool bit=(p[46].w>0.?saved.r:saved.a)>=.999;if(bit!=(p[46].y>0.))discard;}
 vec3 rgb=p[9].w>0.?texture(environment,clamp(envuv,0.,1.)).rgb*tint.rgb:min(vec3(1.),texel.rgb*tint.rgb*(p[10].x>0.?1.:p[10].y)+shine);
 rgb=mix(p[21].xyz,rgb,fog);
 if(p[23].w>0.){float g=dot(rgb,vec3(.299,.587,.114));vec3 graded=clamp((g+(rgb-g)*p[22].w)*p[22].xyz+p[23].x,0.,1.);rgb=mix(rgb,graded,p[23].w);}
 result=vec4(rgb,clamp(alpha,0.,1.)*p[13].x);
}
