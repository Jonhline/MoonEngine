/* Execute the shipped UI vertex/mix shaders with the production full-image quad. */
#include "../../src/platform/sdl/render3d.c"
#include "../../src/platform/switch/deko_composition.h"
#include "../../src/platform/switch/deko_contract.h"
#include <stdio.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "composition line %d: %s\n", __LINE__, #x);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static char *read_shader(const char *directory, const char *name)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", directory, name);
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *text = malloc((size_t)size + 1);
    if (text && fread(text, 1, (size_t)size, file) == (size_t)size)
        text[size] = 0;
    else {
        free(text);
        text = NULL;
    }
    fclose(file);
    return text;
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
        SDL_CreateWindow("composition", 0, 0, 32, 32, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    CHECK(window && SDL_GL_CreateContext(window) && gl_load());
    char *vertex = read_shader(argv[1], "ui.vert"), *fragment = read_shader(argv[1], "mix.frag");
    CHECK(vertex && fragment);
    GLuint program = gl_program(vertex, fragment);
    free(vertex);
    free(fragment);
    CHECK(program);
    GLuint vao, buffer, ubo, target, fbo, layers[4];
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(nd_ui_vertex), NULL, GL_DYNAMIC_DRAW);
    const unsigned offsets[3] = {0, 8, 16}, sizes[3] = {2, 2, 4};
    for (unsigned i = 0; i < 3; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, sizeof(nd_ui_vertex),
                              (void *)(uintptr_t)offsets[i]);
    }
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(nd_uniform), NULL, GL_DYNAMIC_DRAW);
    PFNGLBINDBUFFERBASEPROC bind_base =
        (PFNGLBINDBUFFERBASEPROC)SDL_GL_GetProcAddress("glBindBufferBase");
    CHECK(bind_base);
    bind_base(GL_UNIFORM_BUFFER, 0, ubo);
    glGenTextures(4, layers);
    const uint8_t colors[4][4] = {
        {192, 24, 80, 255}, {16, 176, 48, 255}, {64, 32, 224, 255}, {128, 96, 32, 255}};
    const char *names[4] = {"a", "b", "c", "d"};
    glUseProgram(program);
    for (unsigned i = 0; i < 4; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, layers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(glGetUniformLocation(program, names[i]), i);
    }
    glGenTextures(1, &target);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    const unsigned dimensions[][2] = {
        {640, 448}, {256, 256}, {960, 540}, {1280, 720}, {1920, 1080}};
    const float weights[][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {.25f, .5f, .125f, .125f}};
    unsigned cases = 0;
    for (unsigned size = 0; size < 5; size++) {
        unsigned width = dimensions[size][0], height = dimensions[size][1];
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, target);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
        CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glViewport(0, 0, width, height);
        uint8_t *pixels = malloc((size_t)width * height * 4);
        CHECK(pixels);
        nd_ui_vertex vertices[6];
        nd_full_image_quad(vertices);
        if (negative)
            for (unsigned i = 0; i < 6; i++)
                vertices[i].y *= fminf(1.f, width * (9.f / 16) / height);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof vertices, vertices);
        for (unsigned blend = 0; blend < 3; blend++) {
            nd_uniform uniform = {0};
            memcpy(uniform.v[1], weights[blend], sizeof weights[blend]);
            glBindBuffer(GL_UNIFORM_BUFFER, ubo);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof uniform, &uniform);
            glClearColor(1, 0, 1, 0); /* Any uncovered pixel exposes the old bands. */
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            CHECK(glGetError() == GL_NO_ERROR);
            uint8_t expected[4] = {0, 0, 0, 255};
            for (unsigned channel = 0; channel < 3; channel++) {
                float value = 0;
                for (unsigned i = 0; i < 4; i++)
                    value += colors[i][channel] * weights[blend][i];
                expected[channel] = (uint8_t)floorf(value + .5f);
            }
            for (size_t at = 0; at < (size_t)width * height; at++)
                for (unsigned channel = 0; channel < 4; channel++) {
                    int difference = abs((int)pixels[at * 4 + channel] - expected[channel]);
                    if (difference > 1) {
                        fprintf(stderr,
                                "composition %ux%u x=%zu y=%zu channel=%u actual=%u expected=%u\n",
                                width, height, at % width, at / width, channel,
                                pixels[at * 4 + channel], expected[channel]);
                        return 1;
                    }
                }
            cases++;
        }
        free(pixels);
    }
    printf("native composition: %u full-target RGBA comparisons; all pixels covered\n", cases);
    return 0;
}
