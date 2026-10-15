#include "launcher_fsr.h"
#include "fsr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"launcher FSR %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char **argv)
{
    (void)argv;
    launcher_fsr *f=launcher_fsr_open(); CHECK(f);
    uint8_t source[15*11*4],expected[32*23*4];
    for(int y=0;y<11;y++) for(int x=0;x<15;x++) {
        uint8_t *p=source+4*(y*15+x);
        p[0]=(uint8_t)(x*17); p[1]=(uint8_t)(y*23);
        p[2]=x<y?255:0; p[3]=x<4?0:255;
    }
    /* Independent reference is the existing game FSR backend, same shaders,
       exact RGBA readback on this GPU. It has separate GL resources. */
    GLuint input;
    glGenTextures(1,&input); glBindTexture(GL_TEXTURE_2D,input);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,15,11,0,GL_RGBA,GL_UNSIGNED_BYTE,source);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    GLuint output=fsr_resolve(input,15,11,32,23); CHECK(output);
    glBindTexture(GL_TEXTURE_2D,output);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,expected);
    glDeleteTextures(1,&input);
    uint8_t *actual=launcher_fsr_image(f,source,15,11,32,23); CHECK(actual);
    if(argc>1) actual[0]^=1;
    CHECK(!memcmp(actual,expected,sizeof expected)); free(actual);
    CHECK(!launcher_fsr_image(f,NULL,15,11,32,23));
    CHECK(!launcher_fsr_image(f,source,15,11,0,23));
    launcher_fsr_close(f);
    f=launcher_fsr_open(); CHECK(f);
    actual=launcher_fsr_image(f,source,15,11,32,23); CHECK(actual);
    CHECK(!memcmp(actual,expected,sizeof expected)); free(actual);
    launcher_fsr_close(f);
    puts("PASS launcher FSR: exact game-backend RGBA, transparency, rejection and recreated context");
    return 0;
}
