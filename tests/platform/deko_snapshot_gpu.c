/* Execute the shipped texel-exact snapshot shader against full/cropped bytes. */
#include "../../src/platform/sdl/render3d.c"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "snapshot GPU line %d\n", __LINE__);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    int mask = argc > 2 && !strcmp(argv[2], "--mask");
    int negative = argc > (mask ? 3 : 2);
    CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window =
        SDL_CreateWindow("snapshot", 0, 0, 32, 24, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    CHECK(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    CHECK(context);
    CHECK(gl_load());
    FILE *file = fopen(argv[1], "rb");
    CHECK(file);
    char fragment[4096];
    size_t n = fread(fragment, 1, sizeof fragment - 1, file);
    fclose(file);
    fragment[n] = 0;
    const char *vertex =
        "#version 460\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &vertex, NULL);
    glCompileShader(vs);
    const char *frag = fragment;
    glShaderSource(fs, 1, &frag, NULL);
    glCompileShader(fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    CHECK(linked);
    enum { W = 32, H = 24, N = W * H * 4 };
    unsigned char source[N], old[N], expected[N], actual[N];
    for (unsigned i = 0; i < N; ++i) {
        source[i] = (unsigned char)(i * 37 + i / 4);
        old[i] = (unsigned char)(255 - source[i]);
    }
    GLuint textures[2], framebuffer, vao;
    glGenTextures(2, textures);
    glGenFramebuffers(1, &framebuffer);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, W, H);
    glUseProgram(program);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, source);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    for (unsigned cropped = 0; cropped < 2; ++cropped) {
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, mask ? GL_R8 : GL_RGBA8, W, H, 0, mask ? GL_RED : GL_RGBA,
                     GL_UNSIGNED_BYTE, old);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1], 0);
        CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
        unsigned x = cropped ? 3 : 0, y = cropped ? 5 : 0, w = cropped ? 17 : W,
                 h = cropped ? 11 : H;
        memcpy(expected, old, N);
        for (unsigned j = y; j < y + h; ++j) {
            if (mask) {
                for (unsigned k = x; k < x + w; ++k)
                    expected[j * W + k] = source[(j * W + k) * 4 + 3];
            } else
                memcpy(expected + (j * W + x) * 4, source + (j * W + x) * 4, w * 4);
        }
        glEnable(GL_SCISSOR_TEST);
        glScissor(x + (negative ? 1 : 0), y, w, h);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(0, 0, W, H, mask ? GL_RED : GL_RGBA, GL_UNSIGNED_BYTE, actual);
        CHECK(!memcmp(actual,expected,mask ? W*H : N));
    }
    glDeleteTextures(2, textures);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("RGBA8 snapshot: full/crop exact, outside pixels retained");
    return 0;
}
