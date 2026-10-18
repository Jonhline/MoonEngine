#pragma once
#include <volk.h>
#include <SDL.h>
#include "gl.h"
#include "vk_render.h"
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>

namespace moon {
void check(VkResult result, const char *operation);
struct Buffer {
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr;
    VkDeviceSize size = 0;
};
struct Image {
    VkImage handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkImageView attachment = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    int width = 0, height = 0, levels = 1;
    int min_filter = GL_NEAREST_MIPMAP_LINEAR, mag_filter = GL_LINEAR;
    int wrap_s = GL_REPEAT, wrap_t = GL_REPEAT;
    int max_level = 1000;
};
struct Uniform {
    std::string name, type;
    int count = 1, offset = 0, stride = 0, components = 1, sampler = -1, unit = 0;
};
struct Program {
    std::string vertex, fragment, error;
    VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    std::vector<Uniform> uniforms;
    std::vector<uint8_t> values;
    std::map<std::array<uint64_t, 67>, VkPipeline> pipelines;
    uint64_t fingerprint = 0;
    bool linked = false, vertex_id = false;
    std::array<bool, 16> inputs = {};
};
struct Attribute {
    int size = 4, stride = 0;
    GLenum type = GL_FLOAT;
    bool normalized = false, integer = false, enabled = false;
    size_t offset = 0;
    unsigned buffer = 0;
};
struct VertexArray {
    std::array<Attribute, 16> attributes;
};
struct Framebuffer {
    unsigned color = 0, depth = 0;
    bool color_texture = false;
};
struct State {
    unsigned texture_unit = 0, array_buffer = 0, vao = 0, program = 0, renderbuffer = 0;
    unsigned read_fb = 0, draw_fb = 0;
    std::array<unsigned, 16> textures = {};
    int viewport[4] = {0, 0, 640, 480}, scissor[4] = {0, 0, 640, 480};
    float clear_color[4] = {}, blend_color[4] = {};
    float clear_depth = 1;
    bool blend = false, depth = false, cull = false, clip = false, depth_write = true;
    bool color_write[4] = {true, true, true, true};
    GLenum depth_func = GL_LESS, cull_face = GL_BACK, front_face = GL_CCW;
    GLenum src_rgb = GL_ONE, dst_rgb = GL_ZERO, src_a = GL_ONE, dst_a = GL_ZERO;
    GLenum eq_rgb = GL_FUNC_ADD, eq_a = GL_FUNC_ADD;
    int pack_alignment = 4, unpack_alignment = 4;
};
struct FrameResources {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore acquired = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkQueryPool queries = VK_NULL_HANDLE;
    Buffer arena;
    std::vector<VkFramebuffer> transient_targets;
    std::vector<std::shared_ptr<Image>> retired_images;
    std::vector<Buffer> retired_buffers;
    std::map<std::array<uint64_t, 17>, VkDescriptorSet> draw_sets;
    bool pending = false, timing_written = false;
};
using PipelineRecipe = std::array<uint64_t, 117>;
uint64_t shader_fingerprint(const Program &program);
struct DrawCommands {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkViewport viewport{};
    VkRect2D scissor{};
    std::array<float, 4> blend{};
    std::array<VkBuffer, 16> vertices{};
    std::array<VkDeviceSize, 16> offsets{};
    unsigned bindings = 0;
    bool dynamic_valid = false;
};
struct Context {
    SDL_Window *window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t family = 0;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D extent = {};
    VkFormat surface_format = VK_FORMAT_B8G8R8A8_UNORM;
    std::vector<VkImage> swap_images;
    VkSemaphore acquired = VK_NULL_HANDLE;
    std::vector<VkSemaphore> present_ready;
    VkQueryPool queries = VK_NULL_HANDLE;
    unsigned timestamp_bits = 0;
    bool pending = false, timing_written = false, low_latency = false;
    FrameResources spare;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptors = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties = {};
    Buffer arena, readback;
    size_t arena_used = 0, default_vertex_offset = SIZE_MAX;
    unsigned descriptor_count = 0, next_id = 1;
    unsigned clear_program = 0;
    moon_vk_render_stats statistics = {};
    bool recording = false, vsync = true, rebuild_swapchain = true;
    /* Android destroys the surface when the document picker takes the
       foreground. A known-lost surface is never presented to; the surface and
       swapchain are rebuilt from the current window on the next frame. */
    bool surface_lost = false, surface_error_reported = false;
    bool stream_geometry = false, uncached_descriptors = false, split_present = false,
         transient_framebuffers = false;
    std::map<std::array<uint64_t, 17>, VkDescriptorSet> draw_sets;
    bool repeat_commands = false, profile_draws = false;
    DrawCommands emitted;
    State state;
    std::map<unsigned, std::shared_ptr<Image>> textures, renderbuffers;
    std::map<unsigned, std::vector<uint8_t>> buffers;
    std::map<unsigned, bool> static_buffers;
    std::map<unsigned, Buffer> vertex_buffers;
    std::vector<Buffer> retired_buffers;
    std::map<unsigned, VertexArray> arrays;
    std::map<unsigned, Framebuffer> framebuffers;
    std::map<unsigned, Program> programs;
    std::map<unsigned, std::pair<GLenum, std::string>> shaders;
    std::map<std::array<int, 6>, VkSampler> samplers;
    std::map<std::array<int, 2>, VkRenderPass> passes;
    std::vector<VkFramebuffer> transient_targets;
    std::map<std::array<VkImageView, 2>, VkFramebuffer> target_cache;
    std::vector<std::shared_ptr<Image>> retired_images;
    VkImageView pass_color = VK_NULL_HANDLE, pass_depth = VK_NULL_HANDLE;
    std::shared_ptr<Image> white, screen;
    std::string error, pipeline_cache_path;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    std::set<PipelineRecipe> pipeline_recipes;
    bool warming_recipes = false;
    void read_pipeline_recipes();
    void save_pipeline_recipes();
    void remember_pipeline(const Program &program, VkRenderPass pass, GLenum topology);
    void warm_pipeline_recipes(Program &program);
    void open_pipeline_cache();
    void save_pipeline_cache();
    uint32_t memory_type(uint32_t mask, VkMemoryPropertyFlags flags);
    Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host_visible = true,
                  bool cached = false);
    void destroy(Buffer &buffer);
    void retire_vertex_buffer(unsigned id);
    const Buffer *vertex_buffer(unsigned id);
    std::shared_ptr<Image> image(int width, int height, VkFormat format, int levels = 1);
    void destroy(Image &image);
    void begin();
    void end_pass();
    void flush();
    void reset_submission();
    void open_submission();
    void swap_submission();
    void drain();
    void write_timestamp_end();
    void close_submission();
    void barrier();
    size_t reserve(size_t bytes, size_t alignment = 16);
    Image *color(unsigned framebuffer);
    Image *depth(unsigned framebuffer);
    void upload(Image &image, int level, int x, int y, int width, int height, GLenum format,
                const void *pixels);
    void read(Image &image, int x, int y, int width, int height, GLenum format, void *pixels);
    VkSampler sampler(const Image &image);
    VkRenderPass render_pass(VkFormat color, VkFormat depth);
    void blit(Image &source, Image &dest, int sx0, int sy0, int sx1, int sy1, int dx0, int dy0,
              int dx1, int dy1, bool linear, bool depth = false);
    VkDescriptorSet draw_descriptors(const Program &program);
    VkFramebuffer target_framebuffer(Image &color, Image *depth, VkRenderPass pass);
    void bind_draw_state(VkPipeline pipeline, const VkViewport &viewport, const VkRect2D &clip,
                         unsigned bindings, const std::array<VkBuffer, 16> &vertices,
                         const std::array<VkDeviceSize, 16> &offsets);
    void draw(GLenum topology, int first, int count);
    void present();
    bool resize_swapchain();
    /* Rebuild the Vulkan surface from the current window after surface loss. */
    bool recreate_surface();
    void close();
};
Context &context();
bool compile(Program &program);
VkPipeline pipeline(Context &ctx, Program &program, VkRenderPass pass, GLenum topology);
void uniform(int location, int count, int components, const void *values, bool matrix = false);
} // namespace moon
