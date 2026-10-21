/* P: decode each half-range sRGB environment texel once per source update.
   Hardware then interpolates the linear float image. GGX samples/weights stay
   identical to the reference shader; no roughness or reflection quality cut. */
static struct {
    GLuint program, vao, fbo, texture, source;
    unsigned revision;
    int width, height, valid;
} environment_cache;
static int environment_cache_prepare(const r3d_view *view)
{
    GLuint source = gpu_environment ? gpu_environment : env_tex;
    int width = gpu_environment ? gpu_environment_w : env_w,
        height = gpu_environment ? gpu_environment_h : env_h;
    if (!pc_submission.modern_lighting || !view->modern_light || !view->daylight || !source ||
        width <= 0 || height <= 0 ||
        (getenv("MOON_REFLECTION_REFERENCE") && *getenv("MOON_REFLECTION_REFERENCE") == '1'))
        return 0;
    if (environment_cache.valid && environment_cache.source == source &&
        environment_cache.revision == environment_revision && environment_cache.width == width &&
        environment_cache.height == height)
        return 1;
    if (!environment_cache.program) {
        environment_cache.program = gl_program(
            "#version 330 core\nconst vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));void main(){gl_Position=vec4(p[gl_VertexID],0,1);}",
            "#version 330 core\nout vec4 o;uniform sampler2D image;void main(){vec3 c=clamp(texelFetch(image,ivec2(gl_FragCoord.xy),0).rgb*2.,0.,1.);o=vec4(mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c)),1.);}");
        if (!environment_cache.program)
            return 0;
        glGenVertexArrays(1, &environment_cache.vao);
        glGenFramebuffers(1, &environment_cache.fbo);
        glGenTextures(1, &environment_cache.texture);
    }
    GLint draw, read, viewport[4], active_program, vao;
    GLboolean mask[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &active_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    int depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND),
        cull = glIsEnabled(GL_CULL_FACE), scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glActiveTexture(GL_TEXTURE0 + 7);
    glBindTexture(GL_TEXTURE_2D, environment_cache.texture);
    if (width != environment_cache.width || height != environment_cache.height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glActiveTexture(GL_TEXTURE0 + 8);
    glBindTexture(GL_TEXTURE_2D, source);
    glBindFramebuffer(GL_FRAMEBUFFER, environment_cache.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           environment_cache.texture, 0);
    glViewport(0, 0, width, height);
    glUseProgram(environment_cache.program);
    glUniform1i(glGetUniformLocation(environment_cache.program, "image"), 8);
    glBindVertexArray(environment_cache.vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (depth)
        glEnable(GL_DEPTH_TEST);
    if (blend)
        glEnable(GL_BLEND);
    if (cull)
        glEnable(GL_CULL_FACE);
    if (scissor)
        glEnable(GL_SCISSOR_TEST);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
    glUseProgram(active_program);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    environment_cache.source = source;
    environment_cache.revision = environment_revision;
    environment_cache.width = width;
    environment_cache.height = height;
    environment_cache.valid = 1;
    return 1;
}
static void environment_cache_bind(int enabled)
{
    glUniform1i(U.linear_environment, enabled);
    if (enabled)
        glUniform1i(U.environment_linear, 7); /* persistent reserved unit */
}
