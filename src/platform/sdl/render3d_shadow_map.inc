/* P: a directional depth map for nearby rigid car geometry. Original paths
   do not allocate or bind it. The orthographic design covers 32 metres;
   target resolution and raster bias are PC presentation parameters. */
struct r3d_shadow_map {
    GLuint program, fbo, image, depth;
    GLint projection_uniform, rotation, position, reverse, opacity, texture, texsize, texmode,
        alpha_test, perspective_uniform;
    GLint wrap_uniform, region_uniform, size_uniform;
    int size, height, perspective, batch, target, viewport[4], casters, scissor[4], clipped,
        dithered;
    float projection[16];
};
void r3d_shadow_map_bind(r3d_shadow_map *s)
{
    pc_submission.shadow_map = s && s->casters && r3d_shadow_map_size() ? s : NULL;
}
void r3d_shadow_map_destroy(r3d_shadow_map *s)
{
    if (!s)
        return;
    if (pc_submission.shadow_map == s)
        pc_submission.shadow_map = NULL;
    glDeleteProgram(s->program);
    glDeleteTextures(1, &s->image);
    glDeleteFramebuffers(1, &s->fbo);
    glDeleteRenderbuffers(1, &s->depth);
    free(s);
}
static r3d_shadow_map *shadow_map_create(void)
{
    r3d_shadow_map *s = calloc(1, sizeof *s);
    if (!s)
        return NULL;
    s->program = gl_program(
        "#version 330 core\nlayout(location=0) in vec3 p;layout(location=2)in vec2 uv;layout(location=6)in vec2 shine_alpha;out vec2 coords;out float alpha; uniform mat4 projection;uniform mat3 rot;uniform vec3 pos;uniform float reverse;uniform int perspective;out float depth;"
        "void main(){coords=uv;alpha=shine_alpha.y;vec4 q=projection*vec4(rot*p+pos,1.);depth=q.z;gl_Position=perspective==1?vec4(q.xy*2.-q.ww,reverse>0.?.05:q.w-.1,q.w):vec4(q.xy*2.-1.,reverse>0.?1.-q.z:q.z*2.-1.,1.);}\n",
        "#version 330 core\nin float depth;out vec4 o;uniform float opacity;in vec2 coords;in float alpha;uniform sampler2D image;uniform vec2 texsize;uniform ivec4 texmode;uniform ivec4 alpha_test;uniform ivec2 wrap,size;uniform ivec4 region;"
        "int addr(int at,int size,int mode,int mn,int mx){if(mode==0)return at-size*int(floor(float(at)/float(size)));if(mode==3)return clamp((at&mn)|mx,0,size-1);if(mode==2)return clamp(at,mn,mx);return clamp(at,0,size-1);}"
        "vec4 alphaTexel(){ivec2 p=ivec2(floor(coords*texsize));p=ivec2(addr(p.x,size.x,wrap.x,region.x,region.y),addr(p.y,size.y,wrap.y,region.z,region.w));return texelFetch(image,p,0);}"
        "void main(){if(depth<0.||depth>1.)discard;float a=alpha;if(texmode.w==1&&texmode.x==1){float ta=texmode.z==1?alphaTexel().a*255.:128.;a=ta/128.;if(texmode.y==0)a*=alpha;else if(texmode.y==2)a+=alpha;}a=floor(clamp(a*128.,0.,255.)+.5);if(alpha_test.x==1){int f=alpha_test.y;float r=float(alpha_test.z);bool pass=f==1||(f==2&&a<r)||(f==3&&a<=r)||(f==4&&a==r)||(f==5&&a>=r)||(f==6&&a>r)||(f==7&&a!=r);if(!pass)discard;}if(a<1.)discard;float d=floor(clamp(depth,0.,1.)*16777215.+.5);vec3 bytes=vec3(floor(d/65536.),mod(floor(d/256.),256.),mod(d,256.));o=vec4(bytes/255.,opacity);}\n");
    if (!s->program) {
        free(s);
        return NULL;
    }
    s->projection_uniform = glGetUniformLocation(s->program, "projection");
    s->rotation = glGetUniformLocation(s->program, "rot");
    s->position = glGetUniformLocation(s->program, "pos");
    s->reverse = glGetUniformLocation(s->program, "reverse");
    s->perspective_uniform = glGetUniformLocation(s->program, "perspective");
    s->opacity = glGetUniformLocation(s->program, "opacity");
    s->texture = glGetUniformLocation(s->program, "image");
    s->texsize = glGetUniformLocation(s->program, "texsize");
    s->texmode = glGetUniformLocation(s->program, "texmode");
    s->alpha_test = glGetUniformLocation(s->program, "alpha_test");
    s->wrap_uniform = glGetUniformLocation(s->program, "wrap");
    s->region_uniform = glGetUniformLocation(s->program, "region");
    s->size_uniform = glGetUniformLocation(s->program, "size");
    glGenTextures(1, &s->image);
    glGenFramebuffers(1, &s->fbo);
    glGenRenderbuffers(1, &s->depth);
    return s;
}
int r3d_shadow_map_contains(const r3d_shadow_map *s, const float p[3])
{
    if (!s)
        return 0;
    for (int row = 0; row < 3; row++) {
        float q = s->projection[12 + row];
        for (int col = 0; col < 3; col++)
            q += s->projection[col * 4 + row] * p[col];
        if (q < .2f || q > .8f)
            return 0;
    }
    return 1;
}
int r3d_shadow_map_covers(const float p[3])
{
    return r3d_shadow_map_contains(pc_submission.shadow_map, p);
}
static int shadow_map_begin_target(r3d_shadow_map **owner, const float centre[3],
                                   const float direction[3], int size, int height)
{
    if (!owner || !size)
        return 0;
    float length = sqrtf(direction[0] * direction[0] + direction[1] * direction[1] +
                         direction[2] * direction[2]);
    if (length < 1e-6f)
        return 0;
    if (!*owner)
        *owner = shadow_map_create();
    r3d_shadow_map *s = *owner;
    if (!s)
        return 0;
    pc_submission.shadow_map = NULL;
    s->casters = 0;
    s->perspective = 0;
    s->batch = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s->target);
    memcpy(s->viewport, vp, sizeof vp);
    s->clipped = glIsEnabled(GL_SCISSOR_TEST);
    s->dithered = glIsEnabled(GL_DITHER);
    /* Dithering RGB bytes would corrupt the packed depth. */
    glDisable(GL_DITHER);
    glGetIntegerv(GL_SCISSOR_BOX, s->scissor);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s->image);
    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
    if (s->size != size || s->height != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s->image, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, s->depth);
        glRenderbufferStorage(GL_RENDERBUFFER, gl_reversed_z ? 0x8CAD : GL_DEPTH24_STENCIL8, size,
                              height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  s->depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_FRAMEBUFFER, s->target);
            if (s->dithered)
                glEnable(GL_DITHER);
            return 0;
        }
        s->size = size;
        s->height = height;
    }
    float basis[3][3], up[3] = {0, 1, 0};
    for (int k = 0; k < 3; k++)
        basis[2][k] = direction[k] / length;
    if (fabsf(basis[2][1]) > .95f) {
        up[1] = 0;
        up[2] = 1;
    }
    basis[0][0] = basis[2][1] * up[2] - basis[2][2] * up[1];
    basis[0][1] = basis[2][2] * up[0] - basis[2][0] * up[2];
    basis[0][2] = basis[2][0] * up[1] - basis[2][1] * up[0];
    float rl =
        sqrtf(basis[0][0] * basis[0][0] + basis[0][1] * basis[0][1] + basis[0][2] * basis[0][2]);
    for (int k = 0; k < 3; k++)
        basis[0][k] /= rl;
    basis[1][0] = basis[0][1] * basis[2][2] - basis[0][2] * basis[2][1];
    basis[1][1] = basis[0][2] * basis[2][0] - basis[0][0] * basis[2][2];
    basis[1][2] = basis[0][0] * basis[2][1] - basis[0][1] * basis[2][0];
    memset(s->projection, 0, sizeof s->projection);
    s->projection[15] = 1;
    const float extent = pc_submission.daylight ? 128.f : 32.f, texel = extent / size;
    for (int row = 0; row < 3; row++) {
        float c = 0;
        for (int k = 0; k < 3; k++) {
            c += basis[row][k] * centre[k];
            s->projection[k * 4 + row] = basis[row][k] / extent;
        }
        c = floorf(c / texel + .5f) * texel;
        s->projection[12 + row] = .5f - c / extent;
    }
    glViewport(0, 0, size, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_DEPTH_FUNC_LESS);
    glDepthMask(GL_TRUE);
    glColorMask(1, 1, 1, 1);
    glClearColor(1, 1, 1, 0);
    glClearDepth(GL_DEPTH_FAR);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(s->program);
    glUniformMatrix4fv(s->projection_uniform, 1, GL_FALSE, s->projection);
    glUniform1f(s->reverse, gl_reversed_z != 0);
    glUniform1i(s->perspective_uniform, 0);
    return 1;
}
static int shadow_map_begin_size(r3d_shadow_map **owner, const float centre[3],
                                 const float direction[3], int size)
{
    return shadow_map_begin_target(owner, centre, direction, size, size);
}
int r3d_shadow_map_begin(r3d_shadow_map **owner, const float centre[3], const float direction[3])
{
    return shadow_map_begin_size(owner, centre, direction, r3d_shadow_map_size());
}
static int shadow_caster_inside(const r3d_shadow_map *s, const r3d_view *view, const r3d_group *g,
                                float lamp_planes[6][4])
{
    if (s->perspective)
        return r3d_bounds_visible(lamp_planes, g->lo, g->hi);
    /* The eight world corners are shared by the three axes. */
    float p[8][3];
    for (int corner = 0; corner < 8; corner++)
        for (int r = 0; r < 3; r++) {
            p[corner][r] = view->pos[r];
            for (int c = 0; c < 3; c++)
                p[corner][r] += view->rot[r * 3 + c] * ((corner & (1 << c)) ? g->hi[c] : g->lo[c]);
        }
    for (int axis = 0; axis < 3; axis++) {
        float lo = 2, hi = -1;
        for (int corner = 0; corner < 8; corner++) {
            float q = s->projection[12 + axis];
            for (int r = 0; r < 3; r++)
                q += s->projection[r * 4 + axis] * p[corner][r];
            lo = fminf(lo, q);
            hi = fmaxf(hi, q);
        }
        if (hi < 0 || lo > 1)
            return 0;
    }
    return 1;
}
void r3d_shadow_map_mesh(r3d_shadow_map *s, const r3d_mesh *mesh, const r3d_view *view,
                         float opacity)
{
    if (!s || !mesh || opacity <= 0)
        return;
    glUniformMatrix3fv(s->rotation, 1, GL_TRUE, view->rot);
    glUniform3fv(s->position, 1, view->pos);
    glUniform1f(s->opacity, fminf(1, opacity));
    glBindVertexArray(mesh->vao);
    glUniform1i(s->texture, 0);
    glActiveTexture(GL_TEXTURE0);
    float lamp_planes[6][4];
    if (s->perspective)
        r3d_lamp_frustum(s->projection, view, lamp_planes);
    /* Reflections repeat the surface. Blended glass is not an opaque caster;
       cutout wheels retain their authored alpha rejection and brake branch. */
    for (int i = 0; i < mesh->ngroups; i++) {
        const r3d_group *g = &mesh->groups[i];
        if (g->reflection || (g->blend_enabled && !g->state.alpha_test) ||
            (g->lamp && (g->lamp == 2) != (view->brake != 0)))
            continue;
        if (!shadow_caster_inside(s, view, g, lamp_planes))
            continue;
        int vertices = g->count;
        /* Contiguous groups of one texture and caster test draw as one. */
        if (!s->perspective || s->batch)
            while (i + 1 < mesh->ngroups) {
                const r3d_group *next = &mesh->groups[i + 1];
                if (next->first != g->first + vertices || next->texture != g->texture ||
                    next->reflection || (next->blend_enabled && !next->state.alpha_test) ||
                    next->lamp != g->lamp || next->state.alpha_test != g->state.alpha_test ||
                    (g->state.alpha_test && (next->state.alpha_func != g->state.alpha_func ||
                                             next->state.alpha_ref != g->state.alpha_ref)) ||
                    !shadow_caster_inside(s, view, next, lamp_planes))
                    break;
                vertices += next->count;
                i++;
                frame_stats.merged_groups++;
            }
        const r3d_texture *t =
            g->texture >= 0 && g->texture < mesh->ntextures ? &mesh->textures[g->texture] : NULL;
        glBindTexture(GL_TEXTURE_2D, t ? t->id : 0);
        int k = t ? t->scale : 1;
        glUniform2f(s->texsize, t ? t->tex_w * k : 1, t ? t->tex_h * k : 1);
        glUniform2i(s->size_uniform, t ? t->width * k : 1, t ? t->height * k : 1);
        glUniform2i(s->wrap_uniform, t ? t->wrap_u : 0, t ? t->wrap_v : 0);
        glUniform4i(s->region_uniform, t ? t->min_u * k : 0, t ? t->max_u * k + k - 1 : 0,
                    t ? t->min_v * k : 0, t ? t->max_v * k + k - 1 : 0);
        glUniform4i(s->texmode, t ? t->color_component : 0, t ? t->texture_function : 0,
                    t ? t->native : 0, t != NULL);
        glUniform4i(s->alpha_test, g->state.alpha_test, g->state.alpha_func, g->state.alpha_ref, 0);
        glDrawArrays(GL_TRIANGLES, g->first, vertices);
        s->casters++;
        frame_stats.draws++;
        frame_stats.triangles += vertices / 3;
    }
}
void r3d_shadow_map_end(r3d_shadow_map *s)
{
    glBindFramebuffer(GL_FRAMEBUFFER, s->target);
    glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
    r3d_end();
    glScissor(s->scissor[0], s->scissor[1], s->scissor[2], s->scissor[3]);
    if (s->clipped)
        glEnable(GL_SCISSOR_TEST);
    if (s->dithered)
        glEnable(GL_DITHER);
}
static void shadow_map_receiver(const r3d_view *view)
{
    r3d_shadow_map *s = pc_submission.shadow_map;
    int on = s && s->casters && !view->shadow && !view->ghost;
    glUniform1i(U.shadow_on, on ? 1 + pc_submission.shadow_filter : 0);
    glUniform1i(U.shadow_map, 3);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, on ? s->image : 0);
    glActiveTexture(GL_TEXTURE0);
    if (on)
        glUniformMatrix4fv(U.shadow_projection, 1, GL_FALSE, s->projection);
}

/* P: publish the main camera and resident light map for a later GPU volume pass.
   No readback and no simulation state is modified. */
void r3d_shadow_map_volume(const r3d_view *view)
{
    video_light_volume volume = {0};
    if (view && view->daylight) {
        volume.valid = 1;
        memcpy(volume.camera, view->cam, 12);
        memcpy(volume.right, view->r, 12);
        memcpy(volume.up, view->u, 12);
        memcpy(volume.forward, view->f, 12);
        memcpy(volume.sun, view->sun_direction, 12);
        volume.focal[0] = view->focal / 320 * projection_x;
        volume.focal[1] = (view->focal_y > 0 ? view->focal_y : view->focal) / 224;
        depth_coeffs(view->near_clip > 0 ? view->near_clip : .2f, 20000, volume.clip);
        r3d_shadow_map *map = pc_submission.shadow_map;
        if (map && map->casters) {
            volume.shadow = map->image;
            memcpy(volume.shadow_projection, map->projection, 64);
        }
    }
    video_light_volume_view(&volume);
}
