/* P: single-frame, luminance-directed edge filtering after tone mapping.
   The direction follows the local contrast gradient; a bounded subpixel span
   avoids blurring flat areas. No history, simulation jitter or UI filtering. */
static GLuint edge_aa_resolve(GLuint source, int width, int height)
{
    if (!daylight_post.aa.program) {
        daylight_post.aa.program = gl_program(
            "#version 330 core\nconst vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));out vec2 uv;void main(){gl_Position=vec4(p[gl_VertexID],0,1);uv=p[gl_VertexID]*.5+.5;}",
            "#version 330 core\nin vec2 uv;out vec4 o;uniform sampler2D image;uniform vec2 extent;"
            "float lum(vec3 c){return dot(c,vec3(.299,.587,.114));}"
            "void main(){vec2 pixel=1./extent;vec3 center=texture(image,uv).rgb;"
            "float nw=lum(texture(image,uv+vec2(-1,1)*pixel).rgb),ne=lum(texture(image,uv+vec2(1,1)*pixel).rgb),sw=lum(texture(image,uv+vec2(-1,-1)*pixel).rgb),se=lum(texture(image,uv+vec2(1,-1)*pixel).rgb),m=lum(center);"
            "float lo=min(m,min(min(nw,ne),min(sw,se))),hi=max(m,max(max(nw,ne),max(sw,se)));"
            "if(hi-lo<max(.0312,hi*.125)){o=vec4(center,1);return;}"
            "vec2 direction=vec2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));"
            "float reduce=max((nw+ne+sw+se)*.03125,.0078125);"
            "direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),vec2(-8),vec2(8))*pixel;"
            "vec3 a=.5*(texture(image,uv-direction/6.).rgb+texture(image,uv+direction/6.).rgb);"
            "vec3 b=a*.5+.25*(texture(image,uv-direction*.5).rgb+texture(image,uv+direction*.5).rgb);"
            "float value=lum(b);o=vec4(value<lo||value>hi?a:b,1);}");
        if (!daylight_post.aa.program)
            return source;
        daylight_post.aa.image = glGetUniformLocation(daylight_post.aa.program, "image");
        daylight_post.aa.extent = glGetUniformLocation(daylight_post.aa.program, "extent");
        glGenVertexArrays(1, &daylight_post.aa.vao);
        glGenFramebuffers(1, &daylight_post.aa.fbo);
        glGenTextures(1, &daylight_post.aa.color);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, daylight_post.aa.color);
    if (width != daylight_post.aa.width || height != daylight_post.aa.height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        daylight_post.aa.width = width;
        daylight_post.aa.height = height;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, daylight_post.aa.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           daylight_post.aa.color, 0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glUseProgram(daylight_post.aa.program);
    glBindVertexArray(daylight_post.aa.vao);
    glUniform1i(daylight_post.aa.image, 0);
    glUniform2f(daylight_post.aa.extent, width, height);
    glBindTexture(GL_TEXTURE_2D, source);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    return daylight_post.aa.color;
}
static void edge_aa_destroy(void)
{
    glDeleteProgram(daylight_post.aa.program);
    glDeleteVertexArrays(1, &daylight_post.aa.vao);
    glDeleteFramebuffers(1, &daylight_post.aa.fbo);
    glDeleteTextures(1, &daylight_post.aa.color);
    memset(&daylight_post.aa, 0, sizeof daylight_post.aa);
}
