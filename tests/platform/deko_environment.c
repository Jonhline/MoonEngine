/* Test the native shader against the existing six-face CPU formula. */
/* Compare Switch GPU work with the port's existing CPU formulas, not PS2 pixels. */
#include "../../src/platform/sdl/render3d.c"
#include "switch_render.h"
#include "../../src/platform/switch/deko_contract.h"
#include "plat_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Switch GPU line %d: %s\n", __LINE__, #x);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
enum { FACE = 8, MAP = 32 };
static unsigned char faces[6][FACE * FACE * 4];
static unsigned char reference[MAP * MAP * 4], pixels[MAP * MAP * 4];
static int compare(int negative)
{
    if (negative)
        pixels[0] ^= 128;
    int worst = 0;
    for (unsigned i = 0; i < sizeof pixels; i++) {
        int error = abs((int)pixels[i] - reference[i]);
        if (error > worst)
            worst = error;
    }
    if (worst > 1)
        fprintf(stderr, "maximum channel difference: %d\n", worst);
    return worst <= 1;
}
static void cpu_environment(const float r[3], const float u[3], const float f[3])
{
    static const float axes[6][2][3] = {{{1, 0, 0}, {0, 1, 0}},  {{-1, 0, 0}, {0, 1, 0}},
                                        {{0, 0, 1}, {0, 1, 0}},  {{0, 0, -1}, {0, 1, 0}},
                                        {{0, 1, 0}, {0, 0, -1}}, {{0, -1, 0}, {0, 0, 1}}};
    for (int y = 0; y < MAP; y++)
        for (int x = 0; x < MAP; x++) {
            float mx = ((x + .5f) / MAP - .5f) * 2, my = -((y + .5f) / MAP - .5f) * 2,
                  m2 = mx * mx + my * my;
            if (m2 > 1) {
                float k = 1 / sqrtf(m2);
                mx *= k;
                my *= k;
                m2 = 1;
            }
            float nz = sqrtf(1 - m2), R[3] = {2 * nz * mx, 2 * nz * my, 2 * nz * nz - 1}, D[3];
            for (int k = 0; k < 3; k++)
                D[k] = R[0] * r[k] + R[1] * u[k] - R[2] * f[k];
            int best = 0;
            float big = -1;
            for (int j = 0; j < 6; j++) {
                const float *ff = axes[j][0];
                float d = D[0] * ff[0] + D[1] * ff[1] + D[2] * ff[2];
                if (d > big) {
                    big = d;
                    best = j;
                }
            }
            const float *ff = axes[best][0], *uu = axes[best][1];
            float rr[3] = {ff[1] * uu[2] - ff[2] * uu[1], ff[2] * uu[0] - ff[0] * uu[2],
                           ff[0] * uu[1] - ff[1] * uu[0]};
            float qx = (D[0] * rr[0] + D[1] * rr[1] + D[2] * rr[2]) / big,
                  qy = (D[0] * uu[0] + D[1] * uu[1] + D[2] * uu[2]) / big;
            float fx = fminf(FACE - 1, fmaxf(0, FACE / 2 + qx * FACE / 2 - .5f)),
                  fy = fminf(FACE - 1, fmaxf(0, FACE / 2 - qy * FACE / 2 - .5f));
            int x0 = (int)fx, y0 = (int)fy, x1 = x0 + 1 < FACE ? x0 + 1 : x0,
                y1 = y0 + 1 < FACE ? y0 + 1 : y0;
            float wx = fx - x0, wy = fy - y0;
            for (int c = 0; c < 3; c++) {
                const unsigned char *p = faces[best];
                float a = p[(y0 * FACE + x0) * 4 + c] * (1 - wx) + p[(y0 * FACE + x1) * 4 + c] * wx;
                float b = p[(y1 * FACE + x0) * 4 + c] * (1 - wx) + p[(y1 * FACE + x1) * 4 + c] * wx;
                reference[(y * MAP + x) * 4 + c] = (unsigned char)(a * (1 - wy) + b * wy + .5f);
            }
            reference[(y * MAP + x) * 4 + 3] = 255;
        }
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    int negative = argc > 2;
    CHECK(!SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window =
        SDL_CreateWindow("native environment", 0, 0, 32, 32, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    CHECK(window && SDL_GL_CreateContext(window) && gl_load());
    FILE *file = fopen(argv[1], "rb");
    CHECK(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *fragment = malloc((size_t)size + 1);
    CHECK(fragment && fread(fragment, 1, size, file) == (size_t)size);
    fragment[size] = 0;
    fclose(file);
    const char *vertex = "#version 460\nlayout(location=0) out vec2 uv;"
                         "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
                         "gl_Position=vec4(p*2.-1.,0.,1.);uv=vec2(p.x,1.-p.y);}";
    GLuint program = gl_program(vertex, fragment);
    free(fragment);
    CHECK(program);
    unsigned char atlas[6 * FACE * FACE * 4];
    for (int face = 0; face < 6; face++)
        for (int y = 0; y < FACE; y++)
            for (int x = 0; x < FACE; x++) {
                unsigned char *pixel = faces[face] + (y * FACE + x) * 4;
                pixel[0] = face * 31 + x * 3;
                pixel[1] = 10 + y * 23;
                pixel[2] = 240 - face * 23 - y * 7;
                pixel[3] = 255;
                memcpy(atlas + (y * FACE * 6 + face * FACE + x) * 4, pixel, 4);
            }
    GLuint source, target, framebuffer, uniform, vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenTextures(1, &source);
    glBindTexture(GL_TEXTURE_2D, source);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FACE * 6, FACE, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, MAP, MAP, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glGenBuffers(1, &uniform);
    glBindBuffer(GL_UNIFORM_BUFFER, uniform);
    PFNGLBINDBUFFERBASEPROC bind_base =
        (PFNGLBINDBUFFERBASEPROC)SDL_GL_GetProcAddress("glBindBufferBase");
    CHECK(bind_base);
    bind_base(GL_UNIFORM_BUFFER, 0, uniform);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "atlas"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glViewport(0, 0, MAP, MAP);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    const float bases[3][3][3] = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                                  {{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}},
                                  {{1, 0, 0}, {0, 0, 1}, {0, -1, 0}}};
    unsigned char raw[sizeof pixels];
    for (unsigned pose = 0; pose < 3; pose++) {
        nd_uniform u = {0};
        u.v[0][0] = FACE;
        u.v[0][1] = MAP;
        for (unsigned axis = 0; axis < 3; axis++)
            memcpy(u.v[axis + 1], bases[pose][axis], 12);
        if (negative)
            u.v[2][1] = -u.v[2][1];
        glBufferData(GL_UNIFORM_BUFFER, sizeof u, &u, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(0, 0, MAP, MAP, GL_RGBA, GL_UNSIGNED_BYTE, raw);
        for (int y = 0; y < MAP; y++)
            memcpy(pixels + y * MAP * 4, raw + (MAP - 1 - y) * MAP * 4, MAP * 4);
        cpu_environment(bases[pose][0], bases[pose][1], bases[pose][2]);
        CHECK(compare(0));
    }
    glDeleteBuffers(1, &uniform);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &source);
    glDeleteTextures(1, &target);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    plat_video_shutdown();
    SDL_Quit();
    puts("Native environment shader: three asymmetric six-face poses, <=1 byte/channel PASS");
    return 0;
}
