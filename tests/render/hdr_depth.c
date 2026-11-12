/* P: verify the actual HDR depth transport, including stencil format matching.
   Including the implementation exposes the private target without adding a
   production diagnostic API. The original game has no desktop HDR resolve. */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "../../src/platform/sdl/video_gl.c"
#include <math.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "HDR depth line %d: %s\n", __LINE__, #x);                              \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    GLuint program = gl_program(
        "#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}",
        "#version 330 core\nout vec4 o;void main(){gl_FragDepth=gl_FragCoord.x<64.?.25:.75;o=vec4(.2,.2,.2,1.);}");
    CHECK(program);
    GLuint vao;
    glGenVertexArrays(1, &vao);
    const int modes[] = {0, 2, 3, 4};
    for (unsigned mode = 0; mode < sizeof modes / sizeof *modes; mode++) {
        video_hdr_options(1, 0);
        video_ambient_occlusion(0);
        video_scene_quality(modes[mode], 2);
        plat_video_begin_frame();
        video_begin_scene();
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_TRUE);
        glColorMask(1, 1, 1, 1);
        glUseProgram(program);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        video_end_scene();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, daylight_post.depth);
        if (argc > 1) {
            int vulkan =
                getenv("MOON_GRAPHICS_API") && !strcmp(getenv("MOON_GRAPHICS_API"), "vulkan");
            glTexImage2D(GL_TEXTURE_2D, 0, vulkan ? GL_RGBA8 : 0x8CAC, 128, 128, 0,
                         vulkan ? GL_RGBA : GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
        }
        GLint format = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
        CHECK(format==(gl_reversed_z?0x8CAD:GL_DEPTH24_STENCIL8));
        int width=daylight_post.width,height=daylight_post.height;
        printf("depth target %dx%d scene %dx%d\n",width,height,scene_w,scene_h);
        float *pixels=malloc((size_t)width*height*sizeof *pixels);
        CHECK(pixels);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, pixels);
        CHECK(glGetError()==GL_NO_ERROR);
        float maximum = 0;
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                float error = fabsf(pixels[y * width + x] - (x < 64 ? .25f : .75f));
                CHECK(isfinite(error));
                if (error > maximum)
                    maximum = error;
            }
        printf("HDR depth AA=%d samples=%d format=0x%x max error=%.9g\n", modes[mode],
               scene_samples, format, maximum);
        CHECK(maximum<.000002f);
        plat_video_end_frame();
    }
    glDeleteProgram(program);
    glDeleteVertexArrays(1, &vao);
    plat_video_shutdown();
    return 0;
}
