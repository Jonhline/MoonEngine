/* Execute the shipped native fragment shader against the existing PC shader.
   This tests GLSL/material rules on a host GPU, not the deko command encoder. */
#include "../../src/platform/sdl/render3d.c"
#include "../../src/platform/switch/deko_contract.h"
#include <stdio.h>

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "native material line %d: %s (%s)\n", __LINE__, #c, SDL_GetError());   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum { Side = 8 };
static const char *native_vertex =
    "#version 460\n"
    "layout(std140,binding=0) uniform Params {vec4 p[64];mat4 bones[20];};\n"
    "layout(location=0) out vec2 uv;layout(location=1) out vec4 tint;\n"
    "layout(location=2) out vec3 shine;layout(location=3) out vec2 envuv;\n"
    "layout(location=4) noperspective out float fog;layout(location=5) out float floor_y;\n"
    "void main(){vec2 x=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
    "gl_Position=vec4(x*2.-1.,0.,1.);uv=p[47].xy;envuv=p[47].xy;"
    "tint=p[48];shine=p[49].xyz;fog=1.;floor_y=0.;}\n";
static const char *reference_vertex =
    "#version 330 core\n"
    "uniform vec2 coord;uniform vec4 color;uniform vec3 spec;\n"
    "out vec2 v_uv,v_envuv;out vec3 v_light,v_spec;out float v_alpha,v_floor;"
    "noperspective out float v_fog;\n"
    "void main(){vec2 x=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
    "gl_Position=vec4(x*2.-1.,0.,1.);v_uv=coord;v_envuv=coord;"
    "v_light=color.rgb;v_alpha=color.a;v_spec=spec;v_floor=0.;v_fog=1.;}\n";

static void i1(GLuint program, const char *name, int value)
{
    glUniform1i(glGetUniformLocation(program, name), value);
}
static void f1(GLuint program, const char *name, float value)
{
    glUniform1f(glGetUniformLocation(program, name), value);
}
static void reference(GLuint program, const nd_uniform *u)
{
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "coord"), u->v[47][0], u->v[47][1]);
    glUniform4f(glGetUniformLocation(program, "color"), u->v[48][0], u->v[48][1], u->v[48][2],
                u->v[48][3]);
    glUniform3fv(glGetUniformLocation(program, "spec"), 1, u->v[49]);
    i1(program, "u_tex", 0);
    i1(program, "u_envtex", 1);
    i1(program, "u_dest", 2);
    i1(program, "u_has_tex", (int)u->v[10][0]);
    i1(program, "u_reflect", (int)u->v[9][3]);
    i1(program, "u_cc", (int)u->v[11][0]);
    i1(program, "u_native", (int)u->v[11][1]);
    i1(program, "u_tfx", (int)u->v[11][2]);
    i1(program, "u_atest", (int)u->v[12][0]);
    i1(program, "u_afunc", (int)u->v[12][1]);
    f1(program, "u_aref", u->v[12][2]);
    i1(program, "u_date", (int)u->v[46][0]);
    i1(program, "u_date_bit", (int)u->v[46][1]);
    i1(program, "u_pass", (int)u->v[46][2]);
    f1(program, "u_fade", u->v[13][0]);
    f1(program, "u_untex", u->v[10][1]);
    i1(program, "u_nearest", (int)u->v[44][2]);
    glUniform2i(glGetUniformLocation(program, "u_size"), (int)u->v[43][2], (int)u->v[43][3]);
    glUniform2f(glGetUniformLocation(program, "u_texsize"), u->v[43][0], u->v[43][1]);
    glUniform2i(glGetUniformLocation(program, "u_wrap"), (int)u->v[44][0], (int)u->v[44][1]);
    glUniform4i(glGetUniformLocation(program, "u_region"), (int)u->v[45][0], (int)u->v[45][1],
                (int)u->v[45][2], (int)u->v[45][3]);
    glUniform2i(glGetUniformLocation(program, "u_envsize"), 4, 4);
}
static GLuint texture(int w, int h, const uint8_t *pixels)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return id;
}
/* Capture the shipped vertex shader's lighting outputs through a fragment
   shader. Geometry has equal depth, so the backend-specific depth range does
   not affect this comparator. Model transforms and lighting use PC inputs. */
static int vertices(const char *fragment_path, int negative)
{
    char path[1024];
    CHECK(strlen(fragment_path) < sizeof path);
    strcpy(path, fragment_path);
    char *suffix = strstr(path, "mesh.frag");
    CHECK(suffix);
    strcpy(suffix, "mesh.vert");
    FILE *file = fopen(path, "rb");
    CHECK(file);
    CHECK(!fseek(file, 0, SEEK_END));
    long length = ftell(file);
    rewind(file);
    char *code = malloc((size_t)length + 1);
    CHECK(code && fread(code, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    code[length] = 0;
    const char *capture_pc =
        "#version 330 core\n"
        "in vec3 v_light,v_spec;in float v_alpha;noperspective in float v_fog;"
        "uniform int output_kind;out vec4 color;void main(){"
        "color=output_kind==0?vec4(v_light,v_alpha):output_kind==1?vec4(v_spec,1.):output_kind==2?vec4(vec3(v_fog),1.):vec4(vec3(gl_FragCoord.z),1.);}";
    const char *capture_native =
        "#version 460\n"
        "layout(location=1) in vec4 tint;layout(location=2) in vec3 shine;"
        "layout(location=4) noperspective in float fog;"
        "uniform int output_kind;layout(location=0) out vec4 color;void main(){"
        "color=output_kind==0?tint:output_kind==1?vec4(shine,1.):output_kind==2?vec4(vec3(fog),1.):vec4(vec3(gl_FragCoord.z*2.-1.),1.);}";
    if (negative == 2) {
        /* Reproduce the console failure: replace the entire vertex alpha
           range with one, while retaining the actual production RGB path. */
        const char *original = "tint=vec4(lit,opacity)", *broken = "tint=vec4(lit,1.)";
        char *at = strstr(code, original);
        CHECK(at);
        memmove(at + strlen(broken), at + strlen(original), strlen(at + strlen(original)) + 1);
        memcpy(at, broken, strlen(broken));
    }
    GLuint programs[2] = {gl_program(vs, capture_pc), gl_program(code, capture_native)};
    free(code);
    CHECK(programs[0] && programs[1]);
    float data[3][27] = {0};
    const float xy[3][2] = {{-2, -2}, {6, -2}, {-2, 6}};
    for (unsigned i = 0; i < 3; i++) {
        memcpy(data[i], xy[i], sizeof xy[i]);
        data[i][2] = 2;
        data[i][3] = .2f;
        data[i][4] = .3f;
        data[i][5] = -.9f;
        data[i][8] = .5f;
        data[i][9] = .4f;
        data[i][10] = .3f;
        data[i][11] = .3f;
        data[i][12] = .2f;
        data[i][13] = .1f;
        data[i][14] = .7f;
        data[i][15] = .6f;
        data[i][16] = .5f;
        data[i][18] = 1.2f;
    }
    GLuint buffer, uniform;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_DYNAMIC_DRAW);
    const unsigned offsets[9] = {0, 3, 6, 8, 11, 14, 17, 19, 23};
    const unsigned sizes[9] = {3, 3, 2, 3, 3, 3, 2, 4, 4};
    for (unsigned i = 0; i < 9; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, sizeof data[0],
                              (void *)(uintptr_t)(offsets[i] * sizeof(float)));
    }
    glGenBuffers(1, &uniform);
    glBindBuffer(GL_UNIFORM_BUFFER, uniform);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(nd_uniform), NULL, GL_DYNAMIC_DRAW);
    PFNGLBINDBUFFERBASEPROC bind_base =
        (PFNGLBINDBUFFERBASEPROC)SDL_GL_GetProcAddress("glBindBufferBase");
    bind_base(GL_UNIFORM_BUFFER, 0, uniform);
    nd_uniform u = {0};
    u.v[0][0] = u.v[1][1] = u.v[2][2] = 1;
    u.v[5][0] = u.v[6][1] = u.v[7][2] = 1;
    u.v[8][0] = u.v[8][1] = 1;
    u.v[8][2] = .2f;
    u.v[9][1] = .8f;
    u.v[10][2] = .45f;
    u.v[16][0] = .1f;
    u.v[16][1] = .2f;
    u.v[16][2] = .3f;
    u.v[16][3] = .7f;
    u.v[17][0] = .8f;
    u.v[17][1] = .7f;
    u.v[17][2] = .6f;
    u.v[17][3] = 1;
    u.v[18][0] = .1f;
    u.v[19][0] = .75f;
    u.v[19][1] = -.2f;
    u.v[20][0] = 1;
    u.v[20][1] = .3f;
    u.v[20][3] = 1;
    for (unsigned i = 0; i < 2; i++) {
        u.v[24 + i][0] = .2f;
        u.v[24 + i][1] = .4f;
        u.v[24 + i][2] = -.8f;
        for (unsigned k = 0; k < 3; k++) {
            u.v[28 + i][k] = .1f;
            u.v[32 + i][k] = .3f + k * .1f;
            u.v[36 + i][k] = .25f;
        }
    }
    unsigned cases = 0, max_error = 0;
    for (unsigned mode = 0; mode < 3; mode++)
        for (unsigned lights = 0; lights < 3; lights++)
            for (unsigned kind = 0; kind < 4; kind++)
                for (unsigned power = 0; power < 3; power++)
                    for (unsigned opacity = 0; opacity < 5; opacity++) {
                        /* Fractional alpha caught a native vertex regression that
                       the original always-1.2 fixture could not distinguish. */
                        static const float alphas[5] = {.25f, .5f, .75f, 1.2f, 1.5f};
                        for (unsigned vertex = 0; vertex < 3; vertex++)
                            data[vertex][18] = alphas[opacity];
                        u.v[9][0] = (float)mode;
                        u.v[9][2] = kind == 1;
                        u.v[9][3] = kind == 2;
                        u.v[10][3] = (float)lights;
                        u.v[14][1] = kind == 3;
                        u.v[14][3] = lights == 2 ? 1 : 0;
                        for (unsigned i = 0; i < 3; i++)
                            data[i][17] = power == 0 ? 4 : power == 1 ? 8 : 16;
                        glBindBuffer(GL_ARRAY_BUFFER, buffer);
                        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof data, data);
                        float bias = power == 0 ? 0 : power == 1 ? .008f : .01f;
                        u.v[8][2] = .2f + bias;
                        u.v[8][3] = 20000 + bias;
                        for (unsigned output = 0; output < 4; output++) {
                            uint8_t pixels[2][Side * Side * 4];
                            for (unsigned which = 0; which < 2; which++) {
                                GLuint program = programs[which];
                                glUseProgram(program);
                                i1(program, "output_kind", output);
                                if (which) {
                                    nd_uniform input = u;
                                    if (negative == 1)
                                        input.v[9][1] = .05f;
                                    glBindBuffer(GL_UNIFORM_BUFFER, uniform);
                                    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof input, &input);
                                } else {
                                    const float identity[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
                                    glUniformMatrix3fv(glGetUniformLocation(program, "u_rot"), 1,
                                                       GL_TRUE, identity);
                                    const char *names[3] = {"u_r", "u_u", "u_f"};
                                    for (unsigned i = 0; i < 3; i++)
                                        glUniform3fv(glGetUniformLocation(program, names[i]), 1,
                                                     u.v[5 + i]);
                                    glUniform2f(glGetUniformLocation(program, "u_focal"), 1, 1);
                                    glUniform2f(glGetUniformLocation(program, "u_clip"),
                                                (u.v[8][3] + u.v[8][2]) / (u.v[8][3] - u.v[8][2]),
                                                -2 * u.v[8][3] * u.v[8][2] /
                                                    (u.v[8][3] - u.v[8][2]));
                                    i1(program, "u_mode", mode);
                                    i1(program, "u_flat", kind == 1);
                                    i1(program, "u_reflect", kind == 2);
                                    i1(program, "u_specular_only", kind == 3);
                                    i1(program, "u_nlights", lights);
                                    f1(program, "u_lvertex", u.v[14][3]);
                                    f1(program, "u_lspec_scale", u.v[10][2]);
                                    f1(program, "u_cscale", u.v[9][1]);
                                    f1(program, "u_shadow_alpha", u.v[16][3]);
                                    glUniform3fv(glGetUniformLocation(program, "u_shadow_rgb"), 1,
                                                 u.v[16]);
                                    i1(program, "u_colorop", 1);
                                    glUniform3fv(glGetUniformLocation(program, "u_colormul"), 1,
                                                 u.v[17]);
                                    glUniform3fv(glGetUniformLocation(program, "u_coloradd"), 1,
                                                 u.v[18]);
                                    glUniform2f(glGetUniformLocation(program, "u_envk"), u.v[19][0],
                                                u.v[19][1]);
                                    glUniform3f(glGetUniformLocation(program, "u_fog"), 255 - 76.5f,
                                                76.6f, 1);
                                    const char *lnames[4] = {"u_ldir", "u_lamb", "u_ldif",
                                                             "u_lspe"};
                                    for (unsigned j = 0; j < 4; j++) {
                                        float values[12];
                                        for (unsigned i = 0; i < 4; i++)
                                            memcpy(values + i * 3, u.v[24 + j * 4 + i], 12);
                                        glUniform3fv(glGetUniformLocation(program, lnames[j]), 4,
                                                     values);
                                    }
                                }
                                glDrawArrays(GL_TRIANGLES, 0, 3);
                                glReadPixels(0, 0, Side, Side, GL_RGBA, GL_UNSIGNED_BYTE,
                                             pixels[which]);
                            }
                            CHECK(glGetError()==GL_NO_ERROR);
                            for (unsigned i = 0; i < sizeof pixels[0]; i++) {
                                unsigned delta = abs((int)pixels[0][i] - (int)pixels[1][i]);
                                if (delta > max_error)
                                    max_error = delta;
                                if (delta > 1) {
                                    fprintf(
                                        stderr,
                                        "vertex mode=%u lights=%u kind=%u power=%u output=%u byte=%u PC=%u native=%u\n",
                                        mode, lights, kind, power, output, i, pixels[0][i],
                                        pixels[1][i]);
                                    return 1;
                                }
                            }
                            cases++;
                        }
                    }
    printf(
        "native vertex GLSL: %u light/specular/fog/depth-bias cases; max difference %u byte/channel\n",
        cases, max_error);
    glDeleteProgram(programs[0]);
    glDeleteProgram(programs[1]);
    glDeleteBuffers(1, &buffer);
    glDeleteBuffers(1, &uniform);
    return 0;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    CHECK(!SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("native material test", 0, 0, 32, 32,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    CHECK(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    CHECK(context && gl_load());
    PFNGLBINDBUFFERBASEPROC bind_base =
        (PFNGLBINDBUFFERBASEPROC)SDL_GL_GetProcAddress("glBindBufferBase");
    CHECK(bind_base);
    FILE *file = fopen(argv[1], "rb");
    CHECK(file);
    CHECK(!fseek(file, 0, SEEK_END));
    long length = ftell(file);
    CHECK(length > 0);
    rewind(file);
    char *code = malloc((size_t)length + 1);
    CHECK(code && fread(code, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    code[length] = 0;
    GLuint programs[2] = {gl_program(reference_vertex, fs), gl_program(native_vertex, code)};
    free(code);
    CHECK(programs[0] && programs[1]);
    GLuint vao, buffer, fbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(nd_uniform), NULL, GL_DYNAMIC_DRAW);
    bind_base(GL_UNIFORM_BUFFER, 0, buffer);
    uint8_t pixels[4 * 4 * 4], background[Side * Side * 4];
    const uint8_t alphas[6] = {0, 64, 127, 128, 192, 255};
    for (unsigned y = 0; y < 4; y++)
        for (unsigned x = 0; x < 4; x++) {
            unsigned at = (y * 4 + x) * 4;
            pixels[at] = 31 + x * 53;
            pixels[at + 1] = 27 + y * 47;
            pixels[at + 2] = 17 + (x + y) * 23;
            pixels[at + 3] = alphas[(x + y) % 6];
        }
    for (unsigned i = 0; i < Side * Side; i++) {
        background[i * 4] = 17;
        background[i * 4 + 1] = 29;
        background[i * 4 + 2] = 43;
        background[i * 4 + 3] = (i + i / Side) & 1 ? 255 : 0;
    }
    GLuint image = texture(4, 4, pixels), dest = texture(Side, Side, background);
    GLuint environment = texture(4, 4, pixels), target = texture(Side, Side, background);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glViewport(0, 0, Side, Side);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    nd_uniform u = {0};
    u.v[10][0] = u.v[10][1] = u.v[11][0] = u.v[11][1] = u.v[13][0] = 1;
    u.v[43][0] = u.v[43][1] = 8;
    u.v[43][2] = u.v[43][3] = 4;
    u.v[45][0] = u.v[45][2] = 1;
    u.v[45][1] = u.v[45][3] = 2;
    u.v[48][0] = .7f;
    u.v[48][1] = .8f;
    u.v[48][2] = .9f;
    u.v[48][3] = .75f;
    unsigned cases = 0, max_error = 0;
    uint8_t reference_pixels[sizeof background], actual[sizeof background];
    for (unsigned material = 0; material < 3; material++)
        for (unsigned mode = 0; mode < 16; mode++)
            for (unsigned sample = 0; sample < 8; sample++)
                for (unsigned alpha = 0; alpha < 8; alpha++) {
                    u.v[9][3] = material == 1;
                    nd_texture_extent(&u, material == 2 ? 0 : 8, material == 2 ? 0 : 8, 4, 4);
                    u.v[44][0] = mode & 3;
                    u.v[44][1] = mode >> 2;
                    u.v[44][2] = sample & 1;
                    u.v[47][0] = -.3f + sample * .23f;
                    u.v[47][1] = 1.2f - sample * .19f;
                    u.v[11][2] = alpha & 3;
                    u.v[12][0] = alpha > 1;
                    u.v[12][1] = alpha;
                    u.v[12][2] = sample & 1 ? 200 : 64;
                    u.v[46][0] = sample >= 4;
                    u.v[46][1] = sample & 1;
                    u.v[46][2] = sample & 2 ? 2 : 1;
                    for (unsigned which = 0; which < 2; which++) {
                        glActiveTexture(GL_TEXTURE3);
                        glBindTexture(GL_TEXTURE_2D, target);
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Side, Side, GL_RGBA,
                                        GL_UNSIGNED_BYTE, background);
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, image);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                        (mode & 3) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                        (mode >> 2) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
                        glActiveTexture(GL_TEXTURE1);
                        glBindTexture(GL_TEXTURE_2D, environment);
                        glActiveTexture(GL_TEXTURE2);
                        glBindTexture(GL_TEXTURE_2D, dest);
                        if (!which)
                            reference(programs[0], &u);
                        else {
                            nd_uniform input = u;
                            if (argc > 2 && !strcmp(argv[2], "--date-negative"))
                                input.v[46][1] = 1 - input.v[46][1];
                            if (argc > 2 && !strcmp(argv[2], "--extent-negative"))
                                input.v[43][0] = input.v[43][1] = 0;
                            if (argc > 2 && !strcmp(argv[2], "--address-negative"))
                                input.v[44][0] = 0;
                            glUseProgram(programs[1]);
                            glBindBuffer(GL_UNIFORM_BUFFER, buffer);
                            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof input, &input);
                        }
                        glDrawArrays(GL_TRIANGLES, 0, 3);
                        glReadPixels(0, 0, Side, Side, GL_RGBA, GL_UNSIGNED_BYTE,
                                     which ? actual : reference_pixels);
                    }
                    if (argc > 2 && !strcmp(argv[2], "--negative"))
                        actual[0] ^= 128;
                    CHECK(glGetError() == GL_NO_ERROR);
                    for (unsigned i = 0; i < sizeof actual; i++) {
                        unsigned delta = (unsigned)abs((int)actual[i] - (int)reference_pixels[i]);
                        if (delta > max_error)
                            max_error = delta;
                        if (delta > 1) {
                            fprintf(stderr,
                                    "case %u mode=%u sample=%u alpha=%u byte=%u native=%u PC=%u\n",
                                    cases, mode, sample, alpha, i, actual[i], reference_pixels[i]);
                            return 1;
                        }
                    }
                    cases++;
                }
    printf("native GLSL: %u addressing/alpha/DATE cases; max difference %u byte/channel\n", cases,
           max_error);
    int vertex_negative = argc > 2 && !strcmp(argv[2], "--vertex-negative");
    if (argc > 2 && !strcmp(argv[2], "--vertex-alpha-negative"))
        vertex_negative = 2;
    CHECK(!vertices(argv[1], vertex_negative));
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
