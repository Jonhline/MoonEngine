/* Execute native SDK blend states on a host GPU against desktop set_blend.
   This compares the renderer boundary, not GS arithmetic or Maxwell hardware. */
#include "../../src/platform/sdl/render3d.c"
#include <stdio.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "blend line %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int deko_blend_state(unsigned enabled, unsigned equation, int negative, unsigned values[7]);
int main(int argc, char **argv)
{
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    if (argc > 1 && !strcmp(argv[1], "--sdk-separate-negative"))
        negative = 2;
    if (argc > 1 && !strcmp(argv[1], "--sdk-destination-negative"))
        negative = 3;
    CHECK(!SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window =
        SDL_CreateWindow("blend", 0, 0, 8, 8, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    CHECK(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    CHECK(context && gl_load());
    GLuint program = gl_program(
        "#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
        "gl_Position=vec4(p*2.-1.,0.,1.);}",
        "#version 330 core\nuniform vec4 source;out vec4 color;void main(){color=source;}");
    CHECK(program);
    GLuint vao, target, fbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glViewport(0, 0, 8, 8);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glUseProgram(program);
    unsigned cases = 0, max_error = 0;
    for (unsigned enabled = 0; enabled < 2; enabled++)
        for (unsigned equation = 0; equation < 256; equation++)
            for (unsigned fixed = 0; fixed < 4; fixed++)
                for (unsigned sample = 0; sample < 4; sample++) {
                    r3d_group group = {0};
                    group.blend_enabled = (uint8_t)enabled;
                    group.state.blend = equation;
                    group.state.fixed_alpha = fixed * 64;
                    uint8_t pixels[2][4];
                    glUniform4f(glGetUniformLocation(program, "source"), .7f, .2f, .5f,
                                sample / 3.f);
                    for (unsigned which = 0; which < 2; which++) {
                        glClearColor(.15f, .6f, .25f, (3 - sample) / 3.f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        if (!which)
                            set_blend(&group);
                        else {
                            unsigned values[7];
                            CHECK(deko_blend_state(enabled, equation, negative, values));
                            if (values[0])
                                glEnable(GL_BLEND);
                            else
                                glDisable(GL_BLEND);
                            glBlendColor(0, 0, 0, fminf(1, group.state.fixed_alpha / 128.f));
                            glBlendEquationSeparate(values[1], values[2]);
                            glBlendFuncSeparate(values[3], values[4], values[5], values[6]);
                        }
                        glDrawArrays(GL_TRIANGLES, 0, 3);
                        glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels[which]);
                    }
                    GLenum gl_error = glGetError();
                    if (gl_error != GL_NO_ERROR) {
                        fprintf(stderr, "blend enabled=%u equation=%02x GL error=%x\n", enabled,
                                equation, gl_error);
                        return 1;
                    }
                    for (unsigned c = 0; c < 4; c++) {
                        unsigned error = abs((int)pixels[0][c] - (int)pixels[1][c]);
                        if (error > max_error)
                            max_error = error;
                        if (error > 1) {
                            fprintf(
                                stderr,
                                "blend enabled=%u equation=%02x fixed=%u sample=%u channel=%u PC=%u native=%u\n",
                                enabled, equation, fixed, sample, c, pixels[0][c], pixels[1][c]);
                            return 1;
                        }
                    }
                    cases++;
                }
    printf("native blend: %u RGBA cases; maximum difference %u byte/channel\n", cases, max_error);
    glDeleteProgram(program);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &target);
    glDeleteVertexArrays(1, &vao);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
