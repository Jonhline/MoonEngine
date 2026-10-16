/* Shared renderer command vocabulary implemented directly with Vulkan.
   No OpenGL context, driver calls or CPU framebuffer are involved. */
#include "vk_internal.hpp"
#include "vk_render.h"
#include <cstdio>
#include <cmath>

using namespace moon;
static State &s()
{
    return context().state;
}
static Image *texture()
{
    auto &c = context();
    auto found = c.textures.find(s().textures[s().texture_unit]);
    return found == c.textures.end() ? nullptr : found->second.get();
}
static unsigned target(GLenum which)
{
    return which == GL_READ_FRAMEBUFFER ? s().read_fb : s().draw_fb;
}
static void APIENTRY vkglActiveTexture(GLenum unit)
{
    s().texture_unit = std::min(15u, unit - GL_TEXTURE0);
}
static void APIENTRY vkglGenTextures(GLsizei count, GLuint *ids)
{
    while (count--) {
        *ids = context().next_id++;
        context().textures[*ids++] = nullptr;
    }
}
static void APIENTRY vkglBindTexture(GLenum, GLuint id)
{
    s().textures[s().texture_unit] = id;
}
static void APIENTRY vkglDeleteTextures(GLsizei count, const GLuint *ids)
{
    auto &c = context();
    while (count--) {
        auto found = c.textures.find(*ids++);
        if (found != c.textures.end()) {
            c.retired_images.push_back(found->second);
            c.textures.erase(found);
        }
    }
}
static void APIENTRY vkglTexImage2D(GLenum, GLint level, GLint, GLsizei w, GLsizei h, GLint,
                                    GLenum format, GLenum type, const void *pixels)
{
    auto &c = context();
    auto &im = c.textures[s().textures[s().texture_unit]];
    if (type != GL_UNSIGNED_BYTE)
        throw std::runtime_error("Unsupported Vulkan texture input type");
    if (!im || (level == 0 && (im->width != w || im->height != h))) {
        int min = im ? im->min_filter : GL_NEAREST_MIPMAP_LINEAR,
            mag = im ? im->mag_filter : GL_LINEAR;
        int wrap_s = im ? im->wrap_s : GL_REPEAT, wrap_t = im ? im->wrap_t : GL_REPEAT;
        c.retired_images.push_back(im);
        int levels = 1;
        for (int size = std::max(w, h); size > 1; size /= 2)
            levels++;
        im = c.image(w, h, VK_FORMAT_R8G8B8A8_UNORM, levels);
        im->min_filter = min;
        im->mag_filter = mag;
        im->wrap_s = wrap_s;
        im->wrap_t = wrap_t;
    }
    c.upload(*im, level, 0, 0, w, h, format, pixels);
}
static void APIENTRY vkglTexSubImage2D(GLenum, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,
                                       GLenum format, GLenum, const void *pixels)
{
    if (auto *im = texture())
        context().upload(*im, level, x, y, w, h, format, pixels);
}
static void APIENTRY vkglTexParameteri(GLenum, GLenum name, GLint value)
{
    auto *im = texture();
    if (!im)
        return;
    if (name == GL_TEXTURE_MIN_FILTER)
        im->min_filter = value;
    if (name == GL_TEXTURE_MAG_FILTER)
        im->mag_filter = value;
    if (name == GL_TEXTURE_WRAP_S)
        im->wrap_s = value;
    if (name == GL_TEXTURE_WRAP_T)
        im->wrap_t = value;
    if (name == GL_TEXTURE_MAX_LEVEL)
        im->max_level = value;
}
static void APIENTRY vkglTexParameterf(GLenum target, GLenum name, GLfloat value)
{
    vkglTexParameteri(target, name, (int)value);
}
static void APIENTRY vkglGetTexParameteriv(GLenum, GLenum name, GLint *out)
{
    auto *im = texture();
    *out = 0;
    if (!im)
        return;
    if (name == GL_TEXTURE_MIN_FILTER)
        *out = im->min_filter;
    if (name == GL_TEXTURE_MAG_FILTER)
        *out = im->mag_filter;
    if (name == GL_TEXTURE_WRAP_S)
        *out = im->wrap_s;
    if (name == GL_TEXTURE_WRAP_T)
        *out = im->wrap_t;
    if (name == GL_TEXTURE_MAX_LEVEL)
        *out = im->max_level;
}
static void APIENTRY vkglGetTexLevelParameteriv(GLenum, GLint level, GLenum name, GLint *out)
{
    auto *im = texture();
    *out = 0;
    if (!im)
        return;
    *out = name == GL_TEXTURE_WIDTH ? std::max(1, im->width >> level)
                                    : std::max(1, im->height >> level);
}
static void APIENTRY vkglGetTexImage(GLenum, GLint, GLenum format, GLenum, void *pixels)
{
    if (auto *im = texture())
        context().read(*im, 0, 0, im->width, im->height, format, pixels);
}
static void APIENTRY vkglGenerateMipmap(GLenum)
{
    auto *im = texture();
    if (!im)
        return;
    auto &c = context();
    for (int level = 1; level < im->levels; level++) {
        c.barrier();
        VkImageBlit region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)(level - 1), 0, 1};
        region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)level, 0, 1};
        region.srcOffsets[1] = {std::max(1, im->width >> (level - 1)),
                                std::max(1, im->height >> (level - 1)), 1};
        region.dstOffsets[1] = {std::max(1, im->width >> level), std::max(1, im->height >> level),
                                1};
        vkCmdBlitImage(c.command, im->handle, VK_IMAGE_LAYOUT_GENERAL, im->handle,
                       VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_LINEAR);
    }
}
static void APIENTRY vkglGenBuffers(GLsizei n, GLuint *ids)
{
    while (n--) {
        *ids = context().next_id++;
        context().buffers[*ids++] = {};
    }
}
static void APIENTRY vkglDeleteBuffers(GLsizei n, const GLuint *ids)
{
    auto &c = context();
    while (n--) {
        unsigned id = *ids++;
        c.retire_vertex_buffer(id);
        c.buffers.erase(id);
        c.static_buffers.erase(id);
    }
}
static void APIENTRY vkglBindBuffer(GLenum, GLuint buffer)
{
    s().array_buffer = buffer;
}
static void APIENTRY vkglBufferData(GLenum, GLsizeiptr size, const void *data, GLenum usage)
{
    auto &c = context();
    if (size < 0)
        throw std::runtime_error("Vulkan buffer size");
    c.retire_vertex_buffer(s().array_buffer);
    c.static_buffers[s().array_buffer] = usage == GL_STATIC_DRAW;
    auto &buffer = c.buffers[s().array_buffer];
    buffer.resize(size);
    if (data)
        memcpy(buffer.data(), data, size);
}
static void APIENTRY vkglBufferSubData(GLenum, GLintptr offset, GLsizeiptr size, const void *data)
{
    auto &buffer = context().buffers[s().array_buffer];
    if (offset < 0 || size < 0 || (size_t)(offset + size) > buffer.size())
        throw std::runtime_error("Vulkan buffer update bounds");
    auto &c = context();
    /* Initial mesh assembly fills a static allocation with subranges before
       its first draw. Only edits to an uploaded mesh require streaming. */
    if (c.vertex_buffers.find(s().array_buffer) != c.vertex_buffers.end()) {
        c.retire_vertex_buffer(s().array_buffer);
        c.static_buffers[s().array_buffer] = false;
    }
    memcpy(buffer.data() + offset, data, size);
}
static void APIENTRY vkglGenVertexArrays(GLsizei n, GLuint *ids)
{
    while (n--) {
        *ids = context().next_id++;
        context().arrays[*ids++] = {};
    }
}
static void APIENTRY vkglDeleteVertexArrays(GLsizei n, const GLuint *ids)
{
    while (n--)
        context().arrays.erase(*ids++);
}
static void APIENTRY vkglBindVertexArray(GLuint id)
{
    s().vao = id;
}
static void APIENTRY vkglEnableVertexAttribArray(GLuint index)
{
    context().arrays[s().vao].attributes.at(index).enabled = true;
}
static void APIENTRY vkglVertexAttribPointer(GLuint index, GLint size, GLenum type,
                                             GLboolean normalized, GLsizei stride,
                                             const void *pointer)
{
    auto &a = context().arrays[s().vao].attributes.at(index);
    a.size = size;
    a.type = type;
    a.normalized = normalized != 0;
    a.stride = stride;
    a.offset = (size_t)pointer;
    a.buffer = s().array_buffer;
}
static void APIENTRY vkglVertexAttribIPointer(GLuint i, GLint size, GLenum type, GLsizei stride,
                                              const void *p)
{
    vkglVertexAttribPointer(i, size, type, GL_FALSE, stride, p);
    context().arrays[s().vao].attributes.at(i).integer = true;
}
static void APIENTRY vkglGenFramebuffers(GLsizei n, GLuint *ids)
{
    while (n--) {
        *ids = context().next_id++;
        context().framebuffers[*ids++] = {};
    }
}
static void APIENTRY vkglDeleteFramebuffers(GLsizei n, const GLuint *ids)
{
    while (n--)
        context().framebuffers.erase(*ids++);
}
static void APIENTRY vkglBindFramebuffer(GLenum which, GLuint id)
{
    if (which != GL_READ_FRAMEBUFFER)
        s().draw_fb = id;
    if (which != GL_DRAW_FRAMEBUFFER)
        s().read_fb = id;
}
static void APIENTRY vkglGenRenderbuffers(GLsizei n, GLuint *ids)
{
    while (n--) {
        *ids = context().next_id++;
        context().renderbuffers[*ids++] = nullptr;
    }
}
static void APIENTRY vkglDeleteRenderbuffers(GLsizei n, const GLuint *ids)
{
    auto &c = context();
    while (n--) {
        auto found = c.renderbuffers.find(*ids++);
        if (found != c.renderbuffers.end()) {
            c.retired_images.push_back(found->second);
            c.renderbuffers.erase(found);
        }
    }
}
static void APIENTRY vkglBindRenderbuffer(GLenum, GLuint id)
{
    s().renderbuffer = id;
}
static void APIENTRY vkglRenderbufferStorage(GLenum, GLenum format, GLsizei w, GLsizei h)
{
    auto &c = context();
    c.retired_images.push_back(c.renderbuffers[s().renderbuffer]);
    VkFormat vkformat;
    if (format == GL_DEPTH24_STENCIL8 || format == GL_DEPTH_COMPONENT24)
        vkformat = VK_FORMAT_D24_UNORM_S8_UINT;
    else if (format == 0x8CAD /* GL_DEPTH32F_STENCIL8 */ ||
             format == 0x8CAC /* GL_DEPTH_COMPONENT32F */)
        vkformat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    else if (format == GL_RGBA8 || format == GL_RGB8)
        vkformat = VK_FORMAT_R8G8B8A8_UNORM;
    else
        throw std::runtime_error("Unsupported Vulkan renderbuffer format: " +
                                 std::to_string(format));
    c.renderbuffers[s().renderbuffer] = c.image(w, h, vkformat);
}
static void APIENTRY vkglRenderbufferStorageMultisample(GLenum t, GLsizei samples, GLenum format,
                                                        GLsizei w, GLsizei h)
{
    if (samples > 1)
        throw std::runtime_error("Vulkan multisample attachments are not available");
    vkglRenderbufferStorage(t, format, w, h);
}
static void APIENTRY vkglFramebufferRenderbuffer(GLenum which, GLenum attachment, GLenum, GLuint id)
{
    auto &fb = context().framebuffers[target(which)];
    if (attachment == GL_COLOR_ATTACHMENT0) {
        fb.color = id;
        fb.color_texture = false;
    } else
        fb.depth = id;
}
static void APIENTRY vkglFramebufferTexture2D(GLenum which, GLenum attachment, GLenum, GLuint id,
                                              GLint)
{
    auto &fb = context().framebuffers[target(which)];
    if (attachment == GL_COLOR_ATTACHMENT0) {
        fb.color = id;
        fb.color_texture = true;
    }
}
static GLenum APIENTRY vkglCheckFramebufferStatus(GLenum which)
{
    return context().color(target(which)) ? GL_FRAMEBUFFER_COMPLETE
                                          : GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
}
static void APIENTRY vkglBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0,
                                         GLint dy0, GLint dx1, GLint dy1, GLbitfield mask,
                                         GLenum filter)
{
    auto &c = context();
    auto *src = mask & GL_DEPTH_BUFFER_BIT ? c.depth(s().read_fb) : c.color(s().read_fb);
    auto *dst = mask & GL_DEPTH_BUFFER_BIT ? c.depth(s().draw_fb) : c.color(s().draw_fb);
    if (src && dst)
        c.blit(*src, *dst, sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, filter == GL_LINEAR,
               (mask & GL_DEPTH_BUFFER_BIT) != 0);
}
static void APIENTRY vkglCopyTexSubImage2D(GLenum, GLint level, GLint dx, GLint dy, GLint x,
                                           GLint y, GLsizei w, GLsizei h)
{
    auto &c = context();
    auto *src = c.color(s().read_fb), *dst = texture();
    if (!src || !dst)
        return;
    c.barrier();
    VkImageCopy copy{};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)level, 0, 1};
    copy.srcOffset = {x, y, 0};
    copy.dstOffset = {dx, dy, 0};
    copy.extent = {(uint32_t)w, (uint32_t)h, 1};
    vkCmdCopyImage(c.command, src->handle, VK_IMAGE_LAYOUT_GENERAL, dst->handle,
                   VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
}
static void APIENTRY vkglReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum,
                                    void *pixels)
{
    auto &c = context();
    auto *im = format == GL_DEPTH_COMPONENT ? c.depth(s().read_fb) : c.color(s().read_fb);
    if (im)
        c.read(*im, x, y, w, h, format, pixels);
}
static void APIENTRY vkglClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    float value[] = {r, g, b, a};
    memcpy(s().clear_color, value, sizeof value);
}
static void APIENTRY vkglClearDepth(GLdouble value)
{
    s().clear_depth = (float)value;
}
static void APIENTRY vkglClear(GLbitfield mask)
{
    auto &c = context();
    auto *color = c.color(s().draw_fb), *depth = c.depth(s().draw_fb);
    if (!color)
        return;
    if (depth && (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))) {
        VkImageAspectFlags aspect = 0;
        if ((mask & GL_DEPTH_BUFFER_BIT) && s().depth_write)
            aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (mask & GL_STENCIL_BUFFER_BIT)
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        if (aspect) {
            c.barrier();
            VkRenderPass pass = c.render_pass(color->format, depth->format);
            VkImageView views[] = {color->attachment, depth->attachment};
            VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebuffer.renderPass = pass;
            framebuffer.attachmentCount = 2;
            framebuffer.pAttachments = views;
            framebuffer.width = color->width;
            framebuffer.height = color->height;
            framebuffer.layers = 1;
            VkFramebuffer fb;
            check(vkCreateFramebuffer(c.device, &framebuffer, nullptr, &fb), "clear framebuffer");
            c.transient_targets.push_back(fb);
            VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            begin.renderPass = pass;
            begin.framebuffer = fb;
            begin.renderArea.extent = {(uint32_t)color->width, (uint32_t)color->height};
            vkCmdBeginRenderPass(c.command, &begin, VK_SUBPASS_CONTENTS_INLINE);
            VkClearAttachment clear{};
            clear.aspectMask = aspect;
            clear.clearValue.depthStencil = {s().clear_depth, 0};
            VkClearRect rect{{{0, 0}, {(uint32_t)color->width, (uint32_t)color->height}}, 0, 1};
            if (s().clip) {
                int x0 = std::max(0, s().scissor[0]), y0 = std::max(0, s().scissor[1]);
                int x1 = std::min(color->width, s().scissor[0] + s().scissor[2]);
                int y1 = std::min(color->height, s().scissor[1] + s().scissor[3]);
                rect.rect = {{x0, y0},
                             {(uint32_t)std::max(0, x1 - x0), (uint32_t)std::max(0, y1 - y0)}};
            }
            if (rect.rect.extent.width && rect.rect.extent.height)
                vkCmdClearAttachments(c.command, 1, &clear, 1, &rect);
            vkCmdEndRenderPass(c.command);
        }
    }
    if (mask & GL_COLOR_BUFFER_BIT) {
        if (!c.clear_program) {
            c.clear_program = c.next_id++;
            auto &p = c.programs[c.clear_program];
            p.vertex =
                "#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}";
            p.fragment =
                "#version 330 core\nuniform vec4 clear_value;out vec4 color;void main(){color=clear_value;}";
            if (!compile(p))
                throw std::runtime_error(p.error);
        }
        State saved = s();
        s().program = c.clear_program;
        s().blend = s().depth = s().cull = false;
        s().viewport[0] = s().viewport[1] = 0;
        s().viewport[2] = color->width;
        s().viewport[3] = color->height;
        uniform(0, 1, 4, saved.clear_color);
        c.draw(GL_TRIANGLES, 0, 3);
        s() = saved;
    }
}
static void APIENTRY vkglViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    int values[] = {x, y, w, h};
    memcpy(s().viewport, values, sizeof values);
}
static void APIENTRY vkglScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
    int values[] = {x, y, w, h};
    memcpy(s().scissor, values, sizeof values);
}
static void APIENTRY vkglEnable(GLenum cap)
{
    if (cap == GL_BLEND)
        s().blend = true;
    if (cap == GL_DEPTH_TEST)
        s().depth = true;
    if (cap == GL_CULL_FACE)
        s().cull = true;
    if (cap == GL_SCISSOR_TEST)
        s().clip = true;
}
static void APIENTRY vkglDisable(GLenum cap)
{
    if (cap == GL_BLEND)
        s().blend = false;
    if (cap == GL_DEPTH_TEST)
        s().depth = false;
    if (cap == GL_CULL_FACE)
        s().cull = false;
    if (cap == GL_SCISSOR_TEST)
        s().clip = false;
}
static GLboolean APIENTRY vkglIsEnabled(GLenum cap)
{
    return cap == GL_BLEND          ? s().blend
           : cap == GL_DEPTH_TEST   ? s().depth
           : cap == GL_CULL_FACE    ? s().cull
           : cap == GL_SCISSOR_TEST ? s().clip
                                    : GL_FALSE;
}
static void APIENTRY vkglDepthFunc(GLenum value)
{
    s().depth_func = value;
}
static void APIENTRY vkglDepthMask(GLboolean value)
{
    s().depth_write = value != 0;
}
static void APIENTRY vkglCullFace(GLenum value)
{
    s().cull_face = value;
}
static void APIENTRY vkglFrontFace(GLenum value)
{
    s().front_face = value;
}
static void APIENTRY vkglColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    bool values[] = {r != 0, g != 0, b != 0, a != 0};
    memcpy(s().color_write, values, sizeof values);
}
static void APIENTRY vkglBlendFuncSeparate(GLenum src, GLenum dst, GLenum sa, GLenum da)
{
    s().src_rgb = src;
    s().dst_rgb = dst;
    s().src_a = sa;
    s().dst_a = da;
}
static void APIENTRY vkglBlendEquationSeparate(GLenum rgb, GLenum alpha)
{
    s().eq_rgb = rgb;
    s().eq_a = alpha;
}
static void APIENTRY vkglBlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    float values[] = {r, g, b, a};
    memcpy(s().blend_color, values, sizeof values);
}
static void APIENTRY vkglPixelStorei(GLenum name, GLint value)
{
    if (name == GL_PACK_ALIGNMENT)
        s().pack_alignment = value;
    if (name == GL_UNPACK_ALIGNMENT)
        s().unpack_alignment = value;
}
static void APIENTRY vkglFinish()
{
    context().flush();
}
static GLenum APIENTRY vkglGetError()
{
    return context().error.empty() ? GL_NO_ERROR : GL_INVALID_OPERATION;
}
static const GLubyte *APIENTRY vkglGetString(GLenum name)
{
    return (const GLubyte *)(name == GL_RENDERER ? context().properties.deviceName
                                                 : "Moon Engine native Vulkan");
}
static void APIENTRY vkglGetFloatv(GLenum name, GLfloat *out)
{
    if (name == GL_COLOR_CLEAR_VALUE)
        memcpy(out, s().clear_color, sizeof s().clear_color);
    else
        *out = 1;
}
static void APIENTRY vkglGetIntegerv(GLenum name, GLint *out)
{
    if (name == GL_VIEWPORT) {
        memcpy(out, s().viewport, sizeof s().viewport);
        return;
    }
    if (name == GL_SCISSOR_BOX) {
        memcpy(out, s().scissor, sizeof s().scissor);
        return;
    }
    switch (name) {
    case GL_READ_FRAMEBUFFER_BINDING:
        *out = s().read_fb;
        break;
    case GL_DRAW_FRAMEBUFFER_BINDING:
        *out = s().draw_fb;
        break;
    case GL_VERTEX_ARRAY_BINDING:
        *out = s().vao;
        break;
    case GL_CURRENT_PROGRAM:
        *out = s().program;
        break;
    case GL_ACTIVE_TEXTURE:
        *out = GL_TEXTURE0 + s().texture_unit;
        break;
    case GL_TEXTURE_BINDING_2D:
        *out = s().textures[s().texture_unit];
        break;
    case GL_MAJOR_VERSION:
        *out = 4;
        break;
    case GL_MINOR_VERSION:
        *out = 5;
        break;
    case GL_MAX_SAMPLES:
        *out = 1;
        break;
    default:
        *out = 0;
        break;
    }
}
static GLuint APIENTRY vkglCreateShader(GLenum kind)
{
    auto &c = context();
    unsigned id = c.next_id++;
    c.shaders[id] = {kind, ""};
    return id;
}
static void APIENTRY vkglShaderSource(GLuint id, GLsizei count, const GLchar *const *strings,
                                      const GLint *lengths)
{
    auto &source = context().shaders[id].second;
    source.clear();
    for (int i = 0; i < count; i++)
        source.append(strings[i], lengths && lengths[i] >= 0 ? lengths[i] : strlen(strings[i]));
}
static void APIENTRY vkglCompileShader(GLuint)
{
}
static void APIENTRY vkglGetShaderiv(GLuint, GLenum, GLint *out)
{
    *out = GL_TRUE;
}
static void APIENTRY vkglGetShaderInfoLog(GLuint, GLsizei, GLsizei *length, GLchar *out)
{
    if (length)
        *length = 0;
    if (out)
        *out = 0;
}
static void APIENTRY vkglDeleteShader(GLuint id)
{
    context().shaders.erase(id);
}
static GLuint APIENTRY vkglCreateProgram()
{
    unsigned id = context().next_id++;
    context().programs[id] = {};
    return id;
}
static void APIENTRY vkglAttachShader(GLuint id, GLuint shader)
{
    auto &p = context().programs[id];
    auto &sh = context().shaders[shader];
    (sh.first == GL_VERTEX_SHADER ? p.vertex : p.fragment) = sh.second;
}
static void APIENTRY vkglLinkProgram(GLuint id)
{
    compile(context().programs[id]);
}
static void APIENTRY vkglGetProgramiv(GLuint id, GLenum, GLint *out)
{
    *out = context().programs[id].linked;
}
static void APIENTRY vkglGetProgramInfoLog(GLuint id, GLsizei max, GLsizei *length, GLchar *out)
{
    const auto &error = context().programs[id].error;
    int n = std::min((int)error.size(), std::max(0, max - 1));
    if (length)
        *length = n;
    if (max > 0) {
        memcpy(out, error.data(), n);
        out[n] = 0;
    }
}
static void APIENTRY vkglDeleteProgram(GLuint id)
{
    auto &c = context();
    c.flush();
    auto found = c.programs.find(id);
    if (found == c.programs.end())
        return;
    auto &p = found->second;
    for (auto &pipe : p.pipelines)
        vkDestroyPipeline(c.device, pipe.second, nullptr);
    if (p.vs)
        vkDestroyShaderModule(c.device, p.vs, nullptr);
    if (p.fs)
        vkDestroyShaderModule(c.device, p.fs, nullptr);
    if (p.layout)
        vkDestroyPipelineLayout(c.device, p.layout, nullptr);
    c.programs.erase(found);
}
static void APIENTRY vkglUseProgram(GLuint id)
{
    s().program = id;
}
static GLint APIENTRY vkglGetUniformLocation(GLuint id, const GLchar *name)
{
    std::string key = name;
    if (key.size() > 3 && key.substr(key.size() - 3) == "[0]")
        key.resize(key.size() - 3);
    auto &p = context().programs[id];
    for (size_t i = 0; i < p.uniforms.size(); i++)
        if (p.uniforms[i].name == key)
            return (GLint)i;
    return -1;
}
static void APIENTRY vkglUniform1f(GLint l, GLfloat a)
{
    uniform(l, 1, 1, &a);
}
static void APIENTRY vkglUniform1i(GLint l, GLint a)
{
    uniform(l, 1, 1, &a);
}
static void APIENTRY vkglUniform2f(GLint l, GLfloat a, GLfloat b)
{
    float v[] = {a, b};
    uniform(l, 1, 2, v);
}
static void APIENTRY vkglUniform2i(GLint l, GLint a, GLint b)
{
    int v[] = {a, b};
    uniform(l, 1, 2, v);
}
static void APIENTRY vkglUniform3f(GLint l, GLfloat a, GLfloat b, GLfloat c)
{
    float v[] = {a, b, c};
    uniform(l, 1, 3, v);
}
static void APIENTRY vkglUniform4f(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d)
{
    float v[] = {a, b, c, d};
    uniform(l, 1, 4, v);
}
static void APIENTRY vkglUniform4i(GLint l, GLint a, GLint b, GLint c, GLint d)
{
    int v[] = {a, b, c, d};
    uniform(l, 1, 4, v);
}
static void APIENTRY vkglUniform4ui(GLint l, GLuint a, GLuint b, GLuint c, GLuint d)
{
    unsigned v[] = {a, b, c, d};
    uniform(l, 1, 4, v);
}
static void APIENTRY vkglUniform2fv(GLint l, GLsizei n, const GLfloat *v)
{
    uniform(l, n, 2, v);
}
static void APIENTRY vkglUniform3fv(GLint l, GLsizei n, const GLfloat *v)
{
    uniform(l, n, 3, v);
}
static void APIENTRY vkglUniformMatrix3fv(GLint l, GLsizei n, GLboolean transpose, const GLfloat *v)
{
    if (!transpose) {
        uniform(l, n, 3, v, true);
        return;
    }
    std::vector<float> values(n * 9);
    for (int m = 0; m < n; m++)
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 3; col++)
                values[m * 9 + col * 3 + row] = v[m * 9 + row * 3 + col];
    uniform(l, n, 3, values.data(), true);
}
static void APIENTRY vkglUniformMatrix4fv(GLint l, GLsizei n, GLboolean transpose, const GLfloat *v)
{
    if (!transpose) {
        uniform(l, n, 4, v, true);
        return;
    }
    std::vector<float> values(n * 16);
    for (int m = 0; m < n; m++)
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 4; col++)
                values[m * 16 + col * 4 + row] = v[m * 16 + row * 4 + col];
    uniform(l, n, 4, values.data(), true);
}
static void APIENTRY vkglDrawArrays(GLenum topology, GLint first, GLsizei count)
{
    try {
        context().draw(topology, first, count);
        if (getenv("OPENGT4_VK_SYNC"))
            context().flush();
    } catch (const std::exception &e) {
        fprintf(stderr, "Vulkan draw failed: %s (first=%d count=%d)\n", e.what(), first, count);
        throw;
    }
}

/* Do not let C++ allocation/driver errors escape through the C platform API. */
template <auto Function> struct Guard;
template <class R, class... Args, R(APIENTRY *Function)(Args...)> struct Guard<Function> {
    static inline const char *name = "command";
    static R APIENTRY invoke(Args... args)
    {
        try {
            return Function(args...);
        } catch (const std::exception &e) {
            context().error = std::string(name) + ": " + e.what();
            fprintf(stderr, "Vulkan command failed: %s\n", context().error.c_str());
            SDL_SetError("Vulkan: %s", context().error.c_str());
            SDL_Event quit{};
            quit.type = SDL_QUIT;
            SDL_PushEvent(&quit);
            return R();
        }
    }
};

extern "C" void *moon_vk_render_proc(const char *name)
{
    if (!name)
        return nullptr;
#define ENTRY(function)                                                                            \
    if (!strcmp(name, #function)) {                                                                \
        Guard<&vk##function>::name = #function;                                                    \
        return (void *)&Guard<&vk##function>::invoke;                                              \
    }
    ENTRY(glActiveTexture)
    ENTRY(glAttachShader)
    ENTRY(glBindBuffer)
    ENTRY(glBindFramebuffer) ENTRY(glBindRenderbuffer) ENTRY(glBindVertexArray) ENTRY(
        glBlendColor) ENTRY(glBlitFramebuffer) ENTRY(glBlendEquationSeparate)
        ENTRY(glBlendFuncSeparate) ENTRY(glBufferData) ENTRY(glBufferSubData) ENTRY(
            glCheckFramebufferStatus) ENTRY(glCompileShader) ENTRY(glCreateProgram)
            ENTRY(glCreateShader) ENTRY(glDeleteBuffers) ENTRY(glDeleteFramebuffers) ENTRY(
                glDeleteProgram) ENTRY(glDeleteRenderbuffers) ENTRY(glDeleteShader)
                ENTRY(glDeleteVertexArrays) ENTRY(glEnableVertexAttribArray) ENTRY(
                    glFramebufferRenderbuffer) ENTRY(glFramebufferTexture2D) ENTRY(glGenBuffers)
                    ENTRY(glGenerateMipmap) ENTRY(glGenFramebuffers) ENTRY(
                        glGenRenderbuffers) ENTRY(glGenVertexArrays) ENTRY(glGetProgramInfoLog)
                        ENTRY(glGetProgramiv) ENTRY(glGetShaderInfoLog) ENTRY(glGetShaderiv) ENTRY(
                            glGetUniformLocation) ENTRY(glLinkProgram) ENTRY(glRenderbufferStorage)
                            ENTRY(glRenderbufferStorageMultisample) ENTRY(glShaderSource) ENTRY(
                                glUniform1f) ENTRY(glUniform1i) ENTRY(glUniform2f) ENTRY(glUniform2i)
                                ENTRY(glUniform3f) ENTRY(glUniform2fv) ENTRY(glUniform3fv) ENTRY(
                                    glUniform4f) ENTRY(glUniform4i) ENTRY(glUniform4ui)
                                    ENTRY(glUniformMatrix3fv) ENTRY(glUniformMatrix4fv) ENTRY(
                                        glUseProgram) ENTRY(glVertexAttribIPointer)
                                        ENTRY(glVertexAttribPointer) ENTRY(glBindTexture) ENTRY(
                                            glClear) ENTRY(glClearColor) ENTRY(glClearDepth)
                                            ENTRY(glColorMask) ENTRY(glCopyTexSubImage2D) ENTRY(
                                                glCullFace) ENTRY(glDeleteTextures)
                                                ENTRY(glDepthFunc) ENTRY(glDepthMask) ENTRY(
                                                    glDisable) ENTRY(glDrawArrays) ENTRY(glEnable)
                                                    ENTRY(glFinish) ENTRY(glFrontFace) ENTRY(
                                                        glGenTextures) ENTRY(glGetError)
                                                        ENTRY(glGetFloatv) ENTRY(
                                                            glGetIntegerv) ENTRY(glGetString)
                                                            ENTRY(glGetTexImage) ENTRY(
                                                                glGetTexLevelParameteriv)
                                                                ENTRY(glGetTexParameteriv) ENTRY(
                                                                    glIsEnabled) ENTRY(glPixelStorei)
                                                                    ENTRY(glReadPixels) ENTRY(
                                                                        glScissor) ENTRY(glTexImage2D)
                                                                        ENTRY(glTexParameterf) ENTRY(
                                                                            glTexParameteri)
                                                                            ENTRY(glTexSubImage2D)
                                                                                ENTRY(glViewport)
#undef ENTRY
                                                                                    return nullptr;
}
