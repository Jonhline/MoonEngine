/* P: GPU silhouette union, followed by a separable soft edge and a depth-tested
   receiver. This is an optional PC projection, not the original GS shadow pass.
   Ownership belongs to race_view; no per-car allocation, readback or CPU skinning. */
struct r3d_car_shadow {
    GLuint cast, filter, composite, fbo[2], image[2], quad;
    GLint cast_camera[7], receiver_camera[7];
    GLint rotation, position, direction, filter_image, filter_step;
    GLint receiver_image, receiver_step, opacity, tint, reverse;
    int width, height, target, viewport[4];
    float plane[4], camera[3], right[3], up[3], forward[3], focal[2], clip[2];
};

void r3d_car_shadow_destroy(r3d_car_shadow *s)
{
    if (!s)
        return;
    glDeleteProgram(s->cast);
    glDeleteProgram(s->filter);
    glDeleteProgram(s->composite);
    glDeleteFramebuffers(2, s->fbo);
    glDeleteTextures(2, s->image);
    glDeleteVertexArrays(1, &s->quad);
    free(s);
}

static r3d_car_shadow *car_shadow_create(void)
{
    r3d_car_shadow *s = calloc(1, sizeof *s);
    if (!s)
        return NULL;
    const char *quad =
        "#version 330 core\nout vec2 uv; void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
        "uv=p;gl_Position=vec4(p*2.-1.,0.,1.);}\n";
    s->cast = gl_program(
        "#version 330 core\nlayout(location=0) in vec3 p;uniform mat3 rot;"
        "uniform vec3 pos,cam,r,u,f,direction;uniform vec4 plane;uniform vec2 focal,clip;\n"
        "void main(){vec3 w=rot*p+pos;float h=dot(plane.xyz,w)+plane.w;"
        "w-=direction*h/dot(plane.xyz,direction);w-=cam;float z=dot(w,f);"
        "gl_Position=vec4(dot(w,r)*focal.x,dot(w,u)*focal.y,z*clip.x+clip.y,z);}\n",
        "#version 330 core\nout vec4 o;void main(){o=vec4(0.,0.,0.,1.);}\n");
    s->filter = gl_program(
        quad, "#version 330 core\nin vec2 uv;out vec4 o;uniform sampler2D image;uniform vec2 step;"
              "void main(){float a=texture(image,uv).a*.5;"
              "a+=(texture(image,uv-step).a+texture(image,uv+step).a)*.25;o=vec4(0.,0.,0.,a);}\n");
    s->composite = gl_program(
        quad,
        "#version 330 core\nin vec2 uv;out vec4 o;uniform sampler2D image;uniform vec2 step,focal,clip;"
        "uniform vec3 cam,r,u,f,tint;uniform vec4 plane;uniform float opacity,reverse;"
        "void main(){float a=texture(image,uv).a*.5;"
        "a+=(texture(image,uv-step).a+texture(image,uv+step).a)*.25;"
        "if(a<.001)discard;vec2 q=(uv*2.-1.)/focal;vec3 ray=f+r*q.x+u*q.y;"
        "float den=dot(plane.xyz,ray);if(abs(den)<.00001)discard;"
        "float z=-(dot(plane.xyz,cam)+plane.w)/den;if(z<=0.)discard;"
        "float depth=clip.x+clip.y/z;gl_FragDepth=reverse>0.?depth:depth*.5+.5;"
        "o=vec4(tint,a*opacity);}\n");
    if (!s->cast || !s->filter || !s->composite) {
        r3d_car_shadow_destroy(s);
        return NULL;
    }
    const char *camera_fields[] = {"cam", "r", "u", "f", "plane", "focal", "clip"};
    for (int i = 0; i < 7; i++) {
        s->cast_camera[i] = glGetUniformLocation(s->cast, camera_fields[i]);
        s->receiver_camera[i] = glGetUniformLocation(s->composite, camera_fields[i]);
    }
    s->rotation = glGetUniformLocation(s->cast, "rot");
    s->position = glGetUniformLocation(s->cast, "pos");
    s->direction = glGetUniformLocation(s->cast, "direction");
    s->filter_image = glGetUniformLocation(s->filter, "image");
    s->filter_step = glGetUniformLocation(s->filter, "step");
    s->receiver_image = glGetUniformLocation(s->composite, "image");
    s->receiver_step = glGetUniformLocation(s->composite, "step");
    s->opacity = glGetUniformLocation(s->composite, "opacity");
    s->tint = glGetUniformLocation(s->composite, "tint");
    s->reverse = glGetUniformLocation(s->composite, "reverse");
    glGenFramebuffers(2, s->fbo);
    glGenTextures(2, s->image);
    glGenVertexArrays(1, &s->quad);
    return s;
}

static void car_shadow_camera(const GLint fields[7], const r3d_car_shadow *s)
{
    glUniform3fv(fields[0], 1, s->camera);
    glUniform3fv(fields[1], 1, s->right);
    glUniform3fv(fields[2], 1, s->up);
    glUniform3fv(fields[3], 1, s->forward);
    glUniform4f(fields[4], s->plane[0], s->plane[1], s->plane[2], s->plane[3]);
    glUniform2f(fields[5], s->focal[0], s->focal[1]);
    glUniform2f(fields[6], s->clip[0], s->clip[1]);
}

int r3d_car_shadow_begin(r3d_car_shadow **owner, const r3d_view *view, const float plane[4],
                         const float direction[3])
{
    float den = plane[0] * direction[0] + plane[1] * direction[1] + plane[2] * direction[2];
    if (!owner || !view || fabsf(den) < .1f || vp[2] <= 0 || vp[3] <= 0)
        return 0;
    if (!*owner)
        *owner = car_shadow_create();
    r3d_car_shadow *s = *owner;
    if (!s)
        return 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s->target);
    memcpy(s->viewport, vp, sizeof s->viewport);
    memcpy(s->plane, plane, sizeof s->plane);
    memcpy(s->camera, view->cam, sizeof s->camera);
    memcpy(s->right, view->r, sizeof s->right);
    memcpy(s->up, view->u, sizeof s->up);
    memcpy(s->forward, view->f, sizeof s->forward);
    s->focal[0] = view->focal / 320 * projection_x;
    s->focal[1] = (view->focal_y > 0 ? view->focal_y : view->focal) / 224;
    depth_coeffs(view->near_clip > 0 ? view->near_clip : .2f, 20000, s->clip);
    /* Half output resolution; a bounded target avoids multiplying 4K cost per car. */
    int width = vp[2] / 2, height = vp[3] / 2;
    if (width > 1024) {
        height = height * 1024 / width;
        width = 1024;
    }
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;
    glActiveTexture(GL_TEXTURE0);
    if (s->width != width || s->height != height) {
        for (int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, s->image[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, s->fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s->image[i],
                                   0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                glBindFramebuffer(GL_FRAMEBUFFER, s->target);
                return 0;
            }
        }
        s->width = width;
        s->height = height;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo[0]);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s->cast);
    car_shadow_camera(s->cast_camera, s);
    glUniform3fv(s->direction, 1, direction);
    return 1;
}

void r3d_car_shadow_mesh(r3d_car_shadow *s, const r3d_mesh *mesh, const r3d_view *view)
{
    if (!s || !mesh || !view)
        return;
    glUniformMatrix3fv(s->rotation, 1, GL_TRUE, view->rot);
    glUniform3fv(s->position, 1, view->pos);
    glBindVertexArray(mesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertices);
    frame_stats.draws++;
    frame_stats.triangles += mesh->vertices / 3;
}

void r3d_car_shadow_end(r3d_car_shadow *s, float opacity, const float rgb[3])
{
    glUseProgram(s->filter);
    glBindVertexArray(s->quad);
    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo[1]);
    glBindTexture(GL_TEXTURE_2D, s->image[0]);
    glUniform1i(s->filter_image, 0);
    glUniform2f(s->filter_step, 1.f / s->width, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    frame_stats.draws++;
    frame_stats.triangles++;
    glBindFramebuffer(GL_FRAMEBUFFER, s->target);
    glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
    glEnable(GL_SCISSOR_TEST);
    glScissor(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
    glUseProgram(s->composite);
    car_shadow_camera(s->receiver_camera, s);
    glBindTexture(GL_TEXTURE_2D, s->image[1]);
    glUniform1i(s->receiver_image, 0);
    glUniform2f(s->receiver_step, 0, 1.f / s->height);
    glUniform1f(s->opacity, fminf(1, fmaxf(0, opacity)));
    glUniform3fv(s->tint, 1, rgb);
    glUniform1f(s->reverse, gl_reversed_z != 0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_DEPTH_FUNC_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    frame_stats.draws++;
    frame_stats.triangles++;
    r3d_end();
}

/* P: a continuous contact footprint supplements the directional silhouette.
   Dimensions and placement are supplied by the vehicle, not inferred here. */
r3d_mesh *r3d_wheel_contact_create(void)
{
    uint8_t pixels[64 * 64 * 4], alpha[64 * 64];
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            float u = (x + .5f) / 32.f - 1.f;
            float v = (y + .5f) / 32.f - 1.f;
            float radius = u * u + v * v;
            float edge = fminf(1, fmaxf(0, (1 - radius) / .3f));
            float coverage = expf(-3 * radius) * edge * edge * (3 - 2 * edge);
            int at = y * 64 + x;
            memset(pixels + at * 4, 0, 3);
            pixels[at * 4 + 3] = alpha[at] = (uint8_t)floorf(coverage * 128 + .5f);
        }
    }
    tex1_image texture = {.width = 64,
                          .height = 64,
                          .tex_w = 64,
                          .tex_h = 64,
                          .rgba = pixels,
                          .native_alpha = alpha,
                          .color_component = 1,
                          .texture_function = 0,
                          .wrap_u = 1,
                          .wrap_v = 1};
    gt4_triangle triangles[2] = {0};
    const float points[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
    for (int i = 0; i < 2; i++) {
        triangles[i].texture = 0;
        triangles[i].blend_enabled = 1;
        triangles[i].state.blend = 0x44;
        triangles[i].state.depth_test = 1;
        triangles[i].state.depth_func = 2;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *vertex = &triangles[i].v[j];
            vertex->p[0] = points[i * 3 + j][0];
            vertex->p[2] = points[i * 3 + j][1];
            vertex->uv[0] = (vertex->p[0] + 1) * .5f;
            vertex->uv[1] = (vertex->p[2] + 1) * .5f;
            vertex->alpha = 1;
        }
    }
    gt4_model model = {.triangles = triangles, .count = 2, .textures = &texture, .ntextures = 1};
    return r3d_mesh_create(&model);
}
