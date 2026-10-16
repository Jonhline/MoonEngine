/* Synthetic rendering exercises the same command frontend as the game. */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "render3d.h"
#include "gl.h"
#include "vk_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "line %d: %s: %s\n", __LINE__, #condition, SDL_GetError());            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int geometry_fixture(int vulkan)
{
    GLint restore;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &restore);
    GLuint framebuffer, texture, vertices, array;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glViewport(0, 0, 8, 8);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    GLuint program = gl_program(
        "#version 330 core\nlayout(location=0) in vec2 position;void main(){gl_Position=vec4(position,0,1);}",
        "#version 330 core\nout vec4 color;void main(){color=vec4(1,0,0,1);}");
    CHECK(program);
    glUseProgram(program);
    glGenVertexArrays(1, &array);
    glBindVertexArray(array);
    glGenBuffers(1, &vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    float data[] = {9, 100, 100, 9, 100, 100, 9, 100, 100, 9, -1, -1, 9, 3, -1, 9, -1, 3};
    glBufferData(GL_ARRAY_BUFFER, sizeof data, NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof data, data);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)sizeof(float));
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 3, 3);
    moon_vk_render_stats before = moon_vk_render_statistics();
    for (int i = 0; i < 128; i++)
        glDrawArrays(GL_TRIANGLES, 3, 3);
    if (vulkan)
        CHECK(moon_vk_render_statistics().vertex_upload_bytes == before.vertex_upload_bytes);
    uint8_t pixel[4];
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0);
    /* Editing a cached mesh must replace its GPU data; old commands may
       still use the previous version. Move every vertex outside the target. */
    glDrawArrays(GL_TRIANGLES, 3, 3);
    for (int i = 0; i < 18; i++)
        data[i] = 100;
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof data, data);
    glEnable(GL_SCISSOR_TEST);
    glScissor(4, 0, 4, 8);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glDrawArrays(GL_TRIANGLES, 3, 3);
    glDeleteBuffers(1, &vertices);
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 255);
    glReadPixels(2, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0);
    glDeleteVertexArrays(1, &array);
    glDeleteProgram(program);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)restore);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &texture);
    CHECK(glGetError() == GL_NO_ERROR);
    return 0;
}
int main(int argc, char **argv)
{
    CHECK(argc == 3);
    SDL_setenv("MOON_WINDOW_HIDDEN", "1", 1);
    plat_video_desc desc = {320, 240, 0, 0, 0, !strcmp(argv[1], "vulkan")};
    CHECK(plat_video_init(&desc) == 0);
    CHECK(!SDL_GL_GetCurrentContext() == desc.backend);
    CHECK(r3d_available());
    plat_video_set_view(320, 240);
    video_set_menu_pacing(0);
    video_ui_quality(2, 0);
    plat_video_begin_frame();
    const uint8_t red[4] = {220, 20, 30, 255}, green[4] = {20, 230, 40, 255};
    const uint8_t blue[4] = {20, 40, 240, 128}, white[4] = {255, 255, 255, 255};
    plat_draw_quad(NULL, 8, 8, 140, 100, 0, 0, 1, 1, red);
    plat_set_clip(38, 32, 80, 72);
    plat_draw_quad(NULL, 8, 8, 140, 100, 0, 0, 1, 1, blue);
    plat_set_clip(0, 0, 0, 0);
    plat_texture *capture = video_grab(NULL, 8, 8, 140, 100);
    CHECK(capture);
    plat_draw_quad(capture, 8, 120, 140, 100, 0, 1, 1, 0, white);
    uint8_t pixels[4 * 4 * 4];
    for (int i = 0; i < 16; i++)
        memcpy(pixels + i * 4, i & 1 ? red : green, 4);
    plat_texture *image = plat_texture_create(4, 4, pixels);
    CHECK(image);
    plat_draw_quad(image, 160, 8, 144, 100, 0, 0, 1, 1, white);
    video_flush2d();
    /* Preserve RGB while replacing alpha, with a clipped clear. */
    glEnable(GL_SCISSOR_TEST);
    glScissor(12, 130, 100, 80);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0, 0, 0, .25f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    /* Depth, culling, vec3 arrays and transposed matrices in one small shader. */
    GLuint program = gl_program(
        "#version 330 core\nuniform mat3 rotation; uniform vec3 points[3]; uniform float depth; void main(){vec3 p=rotation*points[gl_VertexID];gl_Position=vec4(p.xy,depth,1.);}",
        "#version 330 core\nuniform vec4 color;out vec4 o;void main(){o=color;}");
    CHECK(program);
    glUseProgram(program);
    float rotation[9] = {0, -1, 0, 1, 0, 0, 0, 0, 1};
    float points[9] = {-.8f, -.8f, 0, .8f, -.8f, 0, 0, .8f, 0};
    glUniformMatrix3fv(glGetUniformLocation(program, "rotation"), 1, GL_TRUE, rotation);
    glUniform3fv(glGetUniformLocation(program, "points"), 3, points);
    glViewport(160, 12, 144, 100);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDepthFunc(GL_DEPTH_FUNC_LESS);
    glDisable(GL_BLEND);
    glUniform1f(glGetUniformLocation(program, "depth"), .6f);
    glUniform4f(glGetUniformLocation(program, "color"), .1f, .8f, .3f, 1);
    moon_vk_render_stats before = moon_vk_render_statistics();
    for (int i = 0; i < 128; i++)
        glDrawArrays(GL_TRIANGLES, 0, 3);
    if (desc.backend) {
        moon_vk_render_stats after = moon_vk_render_statistics();
        CHECK(after.draws - before.draws == 128);
        CHECK(after.descriptor_updates - before.descriptor_updates <= 1);
        CHECK(after.passes - before.passes <= 1);
        CHECK(after.submissions == before.submissions);
        /* Destroy an already sampled texture before completion. The image
           must stay alive without forcing a fence wait. */
        plat_texture_destroy(image);
        image = NULL;
        CHECK(moon_vk_render_statistics().submissions == before.submissions);
    }
    glUniform1f(glGetUniformLocation(program, "depth"), .4f);
    glUniform4f(glGetUniformLocation(program, "color"), 1, 0, 1, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_SCISSOR_TEST);
    glScissor(160, 12, 72, 100);
    glClearDepth(GL_DEPTH_FAR);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glUniform1f(glGetUniformLocation(program, "depth"), .5f);
    glUniform4f(glGetUniformLocation(program, "color"), .9f, .1f, .2f, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_DEPTH_TEST);
    CHECK(glGetError() == GL_NO_ERROR);
    int width, height;
    uint8_t *frame = plat_sdl_read_pixels(&width, &height);
    CHECK(frame && width==320 && height==240);
    FILE *f = fopen(argv[2], "wb");
    CHECK(f);
    CHECK(fwrite(frame,4,(size_t)width*height,f)==(size_t)width*height);
    CHECK(!fclose(f));
    free(frame);
    /* Exercise presentation, resize/recreation and a second frame. */
    plat_video_end_frame();
    SDL_SetWindowSize(plat_sdl_window(), 384, 256);
    SDL_PumpEvents();
    plat_video_begin_frame();
    plat_draw_quad(NULL, 0, 0, 320, 240, 0, 0, 1, 1, green);
    plat_video_end_frame();
    CHECK(r3d_begin_target(32));
    uint8_t target[32 * 32 * 4];
    r3d_end_target(target);
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(video_set_upscale(1) >= 0);
    plat_video_begin_frame();
    video_begin_scene();
    glColorMask(1, 1, 1, 1);
    glClearColor(.25f, .5f, .75f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    video_end_scene();
    CHECK(video_fsr_available());
    CHECK(glGetError() == GL_NO_ERROR);
    uint8_t *upscaled = plat_sdl_read_pixels(&width, &height);
    CHECK(upscaled);
    size_t center = ((size_t)(height / 2) * width + width / 2) * 4;
    CHECK(abs(upscaled[center]-64)<=1 && abs(upscaled[center+1]-128)<=1 && abs(upscaled[center+2]-191)<=1);
    free(upscaled);
    plat_video_end_frame();
    CHECK(!geometry_fixture(desc.backend));
    plat_texture_destroy(capture);
    plat_texture_destroy(image);
    glDeleteProgram(program);
    plat_video_shutdown();
    return 0;
}
