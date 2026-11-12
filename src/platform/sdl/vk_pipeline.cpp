#include "vk_internal.hpp"
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>
#include <regex>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace moon {
static int aligned(int value, int alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}
static std::string trim(std::string text)
{
    auto first = text.find_first_not_of(" \t\r\n");
    return first == std::string::npos
               ? ""
               : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
static std::string declarations(Program &p, const std::string &source)
{
    std::regex declaration(R"(uniform\s+(\w+)\s+([^;]+);)");
    std::regex member(R"((\w+)\s*(?:\[\s*(\d+)\s*\])?)");
    for (auto it = std::sregex_iterator(source.begin(), source.end(), declaration);
         it != std::sregex_iterator(); ++it) {
        std::string type = (*it)[1], list = (*it)[2];
        for (auto item = std::sregex_iterator(list.begin(), list.end(), member);
             item != std::sregex_iterator(); ++item) {
            std::string name = (*item)[1];
            auto found = std::find_if(p.uniforms.begin(), p.uniforms.end(),
                                      [&](const Uniform &u) { return u.name == name; });
            if (found != p.uniforms.end())
                continue;
            Uniform uniform;
            uniform.name = name;
            uniform.type = type;
            uniform.count = (*item)[2].matched ? std::stoi((*item)[2]) : 1;
            p.uniforms.push_back(uniform);
        }
    }
    return std::regex_replace(source, declaration, "");
}
static std::string interface_locations(Program &p, std::string source, bool vertex,
                                       std::map<std::string, int> &varyings)
{
    std::regex declaration(
        R"((?:layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*)?\b(in|out)\s+(\w+)\s+(\w+)\s*;)");
    std::string result;
    size_t last = 0;
    int attribute = 0;
    for (auto it = std::sregex_iterator(source.begin(), source.end(), declaration);
         it != std::sregex_iterator(); ++it) {
        auto match = *it;
        bool input = match[2] == "in";
        std::string name = match[4];
        int location;
        if (vertex && input) {
            location = match[1].matched ? std::stoi(match[1]) : attribute++;
            if (location >= 16)
                throw std::runtime_error("Vulkan vertex input limit exceeded");
            p.inputs[location] = true;
        } else if (!vertex && !input)
            location = 0;
        else {
            if (!varyings.count(name))
                varyings[name] = (int)varyings.size();
            location = varyings[name];
        }
        result += source.substr(last, match.position() - last);
        result += "layout(location=" + std::to_string(location) + ") " + match[2].str() + " " +
                  match[3].str() + " " + name + ";";
        last = match.position() + match.length();
    }
    return result + source.substr(last);
}
bool compile(Program &p)
{
    p.fingerprint = shader_fingerprint(p);
    static unsigned compile_program_id;
    p.dump_id = (int)compile_program_id++;
    p.vertex_id = p.vertex.find("gl_VertexID") != std::string::npos;
    /* Both stages share one explicitly laid-out block. Uniform setters and
       shaders therefore use the same offsets, including vec3/matrix arrays. */
    try {
        std::string vertex = declarations(p, p.vertex), fragment = declarations(p, p.fragment);
        int offset = 0, sampler = 0;
        std::string block = "layout(std140,set=0,binding=0) uniform MoonUniforms {\n";
        std::string textures;
        for (auto &u : p.uniforms) {
            if (u.type == "sampler2D") {
                if (sampler >= 8)
                    throw std::runtime_error("Vulkan sampler limit exceeded");
                p.sampler_uniforms[sampler] = (unsigned)(&u - p.uniforms.data());
                u.sampler = sampler++;
                textures += "layout(set=0,binding=" + std::to_string(u.sampler + 1) +
                            ") uniform sampler2D " + u.name + ";\n";
                continue;
            }
            int alignment = 4, size = 4;
            u.components = 1;
            if (u.type.find("vec") != std::string::npos) {
                u.components = u.type.back() - '0';
                size = u.components * 4;
                alignment = u.components == 2 ? 8 : 16;
            } else if (u.type == "mat3" || u.type == "mat4") {
                u.components = u.type.back() - '0';
                alignment = 16;
                size = u.components * 16;
            }
            if (u.count > 1)
                alignment = 16;
            u.offset = aligned(offset, alignment);
            u.stride = u.count > 1 ? aligned(size, 16) : size;
            offset = u.offset + u.stride * u.count;
            block += "layout(offset=" + std::to_string(u.offset) + ") " + u.type + " " + u.name;
            if (u.count > 1)
                block += "[" + std::to_string(u.count) + "]";
            block += ";\n";
        }
        p.sampler_count = (unsigned)sampler;
        if (!offset) {
            block += "vec4 moon_unused;\n";
            offset = 16;
        }
        block += "};\n";
        p.values.resize(aligned(offset, 16));
        std::map<std::string, int> varyings;
        vertex = interface_locations(p, vertex, true, varyings);
        fragment = interface_locations(p, fragment, false, varyings);
        std::regex version(R"(#version[^\n]*\n)");
        vertex = std::regex_replace(vertex, version, "");
        fragment = std::regex_replace(fragment, version, "");
        vertex = std::regex_replace(vertex, std::regex(R"(\bgl_VertexID\b)"), "gl_VertexIndex");
        /* Shared frontend uses ZERO_TO_ONE when reversed depth is enabled. */
        std::string prefix = "#version 450 core\n" + block + textures;
        vertex = prefix + vertex;
        fragment = prefix + fragment;
        p.final_vertex = vertex;
        p.final_fragment = fragment;
        static const bool initialized = glslang::InitializeProcess();
        if (!initialized)
            throw std::runtime_error("glslang initialization failed");
        glslang::TShader vs(EShLangVertex), fs(EShLangFragment);
        const char *v = vertex.c_str(), *f = fragment.c_str();
        vs.setStrings(&v, 1);
        fs.setStrings(&f, 1);
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
        for (auto *shader : {&vs, &fs}) {
            shader->setEnvInput(glslang::EShSourceGlsl, shader->getStage(),
                                glslang::EShClientVulkan, 450);
            shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
            shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
            if (!shader->parse(GetDefaultResources(), 450, false, messages))
                throw std::runtime_error(shader->getInfoLog());
        }
        glslang::TProgram program;
        program.addShader(&vs);
        program.addShader(&fs);
        if (!program.link(messages))
            throw std::runtime_error(program.getInfoLog());
        auto module = [&](EShLanguage stage, VkShaderModule &module) {
            std::vector<unsigned> words;
            glslang::SpvOptions options;
            options.disableOptimizer = true;
            glslang::GlslangToSpv(*program.getIntermediate(stage), words, &options);
            if (stage == EShLangVertex)
                p.vs_words = words;
            else
                p.fs_words = words;
            VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            info.codeSize = words.size() * 4;
            info.pCode = words.data();
            check(vkCreateShaderModule(context().device, &info, nullptr, &module),
                  "vkCreateShaderModule");
        };
        module(EShLangVertex, p.vs);
        module(EShLangFragment, p.fs);
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &context().descriptors;
        check(vkCreatePipelineLayout(context().device, &layout, nullptr, &p.layout),
              "vkCreatePipelineLayout");
        p.linked = true;
        if (const char *dump_dir = getenv("MOON_DUMP_SHADERS")) {
            std::error_code ec;
            std::filesystem::create_directories(dump_dir, ec);
            char stem[64];
            snprintf(stem, sizeof stem, "program-%d", p.dump_id);
            std::filesystem::path base = std::filesystem::u8path(dump_dir) / stem;
            std::ofstream(base.string() + ".glsl", std::ios::binary)
                << "// vertex\n" << p.final_vertex << "\n// fragment\n" << p.final_fragment;
            std::ofstream(base.string() + "-vs.spv", std::ios::binary)
                .write((const char *)p.vs_words.data(), (std::streamsize)p.vs_words.size() * 4);
            std::ofstream(base.string() + "-fs.spv", std::ios::binary)
                .write((const char *)p.fs_words.data(), (std::streamsize)p.fs_words.size() * 4);
            fprintf(stderr, "shader dump: program=%u vert_words=%zu frag_words=%zu\n",
                    p.dump_id, p.vs_words.size(), p.fs_words.size());
        }
        context().warm_pipeline_recipes(p);
        return true;
    } catch (const std::exception &e) {
        p.error = e.what();
        fprintf(stderr, "Vulkan shader: %s\n", e.what());
        return false;
    }
}
VkSampler Context::sampler(Image &im)
{
    std::array<int, 7> key = {im.min_filter, im.mag_filter, im.wrap_s,
                              im.wrap_t,     im.max_level,  im.levels, im.anisotropy};
    /* Each image remembers its last sampler; the shared map is the fallback. */
    if (im.sampler && im.sampler_key == key)
        return im.sampler;
    auto found = samplers.find(key);
    if (found != samplers.end()) {
        im.sampler = found->second;
        im.sampler_key = key;
        return found->second;
    }
    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.magFilter = im.mag_filter == GL_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    info.minFilter = im.min_filter == GL_NEAREST || im.min_filter == GL_NEAREST_MIPMAP_NEAREST ||
                             im.min_filter == GL_NEAREST_MIPMAP_LINEAR
                         ? VK_FILTER_NEAREST
                         : VK_FILTER_LINEAR;
    info.mipmapMode =
        im.min_filter == GL_NEAREST_MIPMAP_LINEAR || im.min_filter == GL_LINEAR_MIPMAP_LINEAR
            ? VK_SAMPLER_MIPMAP_MODE_LINEAR
            : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    auto address = [](int wrap) {
        return wrap == GL_REPEAT            ? VK_SAMPLER_ADDRESS_MODE_REPEAT
               : wrap == GL_MIRRORED_REPEAT ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
               : wrap == GL_CLAMP_TO_BORDER ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
                                            : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    };
    info.anisotropyEnable = sampler_anisotropy && im.anisotropy > 1;
    info.maxAnisotropy = info.anisotropyEnable
        ? std::min((float)im.anisotropy, properties.limits.maxSamplerAnisotropy) : 1.f;
    info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.addressModeU = address(im.wrap_s);
    info.addressModeV = address(im.wrap_t);
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.maxLod = im.min_filter == GL_NEAREST || im.min_filter == GL_LINEAR
                      ? 0.f
                      : (float)std::max(0, std::min(im.max_level, im.levels - 1));
    VkSampler result;
    check(vkCreateSampler(device, &info, nullptr, &result), "vkCreateSampler");
    samplers[key] = result;
    im.sampler = result;
    im.sampler_key = key;
    return result;
}
VkRenderPass Context::render_pass(VkFormat color, VkFormat depth, bool first)
{
    std::array<int, 3> key = {(int)color, (int)depth, first ? 1 : 0};
    auto found = passes.find(key);
    if (found != passes.end())
        return found->second;
    VkAttachmentDescription attachments[2]{};
    for (int i = 0; i < 2; i++) {
        attachments[i].format = i ? depth : color;
        attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp =
            first ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp = attachments[i].loadOp;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].initialLayout = attachments[i].finalLayout =
            i ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
              : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        depth_ref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    if (depth)
        subpass.pDepthStencilAttachment = &depth_ref;
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = depth ? 2 : 1;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    VkRenderPass result;
    check(vkCreateRenderPass(device, &info, nullptr, &result), "vkCreateRenderPass");
    passes[key] = result;
    return result;
}
static VkBlendFactor blend_factor(GLenum factor)
{
    switch (factor) {
    case GL_ZERO:
        return VK_BLEND_FACTOR_ZERO;
    case GL_ONE:
        return VK_BLEND_FACTOR_ONE;
    case GL_SRC_COLOR:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case GL_ONE_MINUS_SRC_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case GL_DST_COLOR:
        return VK_BLEND_FACTOR_DST_COLOR;
    case GL_ONE_MINUS_DST_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case GL_SRC_ALPHA:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case GL_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case GL_ONE_MINUS_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case GL_CONSTANT_ALPHA:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case GL_ONE_MINUS_CONSTANT_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    default:
        throw std::runtime_error("Unsupported Vulkan blend factor");
    }
}
static VkBlendOp blend_op(GLenum op)
{
    return op == GL_FUNC_SUBTRACT           ? VK_BLEND_OP_SUBTRACT
           : op == GL_FUNC_REVERSE_SUBTRACT ? VK_BLEND_OP_REVERSE_SUBTRACT
           : op == GL_MIN                   ? VK_BLEND_OP_MIN
           : op == GL_MAX                   ? VK_BLEND_OP_MAX
                                            : VK_BLEND_OP_ADD;
}
static VkFormat attribute_format(const Attribute &a)
{
    if (!a.enabled)
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    if (a.type == GL_FLOAT) {
        const VkFormat types[] = {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
                                  VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};
        return types[a.size - 1];
    }
    if (a.type == GL_UNSIGNED_BYTE && a.size == 4 && a.normalized)
        return VK_FORMAT_R8G8B8A8_UNORM;
    throw std::runtime_error("Unsupported Vulkan vertex format");
}
VkPipeline pipeline(Context &c, Program &p, VkRenderPass pass, GLenum topology)
{
    auto &s = c.state;
    auto &vao = c.arrays[s.vao];
    /* Fixed shader input slots avoid allocating a vector for every draw. */
    std::array<uint64_t, 67> key = {(uint64_t)pass, topology,      s.blend,      s.depth,
                                    s.cull,         s.depth_write, s.depth_func, s.cull_face,
                                    s.front_face,   s.src_rgb,     s.dst_rgb,    s.src_a,
                                    s.dst_a,        s.eq_rgb,      s.eq_a};
    for (unsigned i = 0; i < 4; i++)
        key[15 + i] = s.color_write[i];
    for (unsigned i = 0; i < 16; i++)
        if (p.inputs[i]) {
            auto &a = vao.attributes[i];
            key[19 + i * 3] = a.enabled;
            key[20 + i * 3] = attribute_format(a);
            key[21 + i * 3] = a.stride;
        }
    c.statistics.pipeline_lookups++;
    if (p.last_pipeline && p.last_key == key)
        return p.last_pipeline;
    auto found = p.pipelines.find(key);
    if (found != p.pipelines.end()) {
        p.last_key = key;
        p.last_pipeline = found->second;
        return found->second;
    }
    c.statistics.pipeline_misses++;
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = p.vs;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = p.fs;
    stages[1].pName = "main";
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    for (unsigned i = 0; i < 16; i++)
        if (p.inputs[i]) {
            const auto &a = vao.attributes[i];
            bindings.push_back(
                {i,
                 a.enabled ? (uint32_t)(a.stride ? a.stride : a.size * (a.type == GL_FLOAT ? 4 : 1))
                           : 0u,
                 VK_VERTEX_INPUT_RATE_VERTEX});
            attributes.push_back({i, i, attribute_format(a), 0});
        }
    VkPipelineVertexInputStateCreateInfo vertex{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex.vertexBindingDescriptionCount = (uint32_t)bindings.size();
    vertex.pVertexBindingDescriptions = bindings.data();
    vertex.vertexAttributeDescriptionCount = (uint32_t)attributes.size();
    vertex.pVertexAttributeDescriptions = attributes.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = topology == GL_TRIANGLE_STRIP ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
                        : topology == GL_TRIANGLE_FAN ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
                                                      : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1;
    raster.cullMode = !s.cull                  ? VK_CULL_MODE_NONE
                      : s.cull_face == GL_BACK ? VK_CULL_MODE_BACK_BIT
                                               : VK_CULL_MODE_FRONT_BIT;
    /* Positive-height Vulkan viewports reverse the frontend's winding test.
       Rows stay bottom-up internally and are flipped only for presentation. */
    raster.frontFace =
        s.front_face == GL_CCW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPipelineMultisampleStateCreateInfo samples{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = s.depth;
    depth.depthWriteEnable = s.depth && s.depth_write;
    depth.depthCompareOp = (VkCompareOp)(s.depth_func - GL_NEVER);
    depth.maxDepthBounds = 1;
    VkPipelineColorBlendAttachmentState color{};
    color.blendEnable = s.blend;
    color.srcColorBlendFactor = blend_factor(s.src_rgb);
    color.dstColorBlendFactor = blend_factor(s.dst_rgb);
    color.srcAlphaBlendFactor = blend_factor(s.src_a);
    color.dstAlphaBlendFactor = blend_factor(s.dst_a);
    color.colorBlendOp = blend_op(s.eq_rgb);
    color.alphaBlendOp = blend_op(s.eq_a);
    for (unsigned i = 0; i < 4; i++)
        if (s.color_write[i])
            color.colorWriteMask |= 1u << i;
    VkPipelineColorBlendStateCreateInfo blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &color;
    VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                               VK_DYNAMIC_STATE_BLEND_CONSTANTS};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 3;
    dynamic.pDynamicStates = states;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertex;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &samples;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = p.layout;
    info.renderPass = pass;
    VkPipeline result;
    uint64_t started = SDL_GetPerformanceCounter();
    check(vkCreateGraphicsPipelines(c.device, c.pipeline_cache, 1, &info, nullptr, &result),
          "vkCreateGraphicsPipelines");
    c.statistics.pipeline_ns += (SDL_GetPerformanceCounter() - started) * 1000000000ull /
                                SDL_GetPerformanceFrequency();
    c.statistics.pipeline_creations++;
    p.pipelines[key] = result;
    p.last_key = key;
    p.last_pipeline = result;
    c.remember_pipeline(p, pass, topology);
    return result;
}
VkDescriptorSet Context::draw_descriptors(const Program &p)
{
    std::array<VkDescriptorImageInfo, 8> images{};
    /* Linking fixes the sampler list. Avoid scanning every material, matrix
       and lamp uniform again for each course draw. Units remain dynamic. */
    unsigned sampler_count = p.sampler_count;
    for (unsigned index = 0; index < sampler_count; index++) {
        const auto &u = p.uniforms[p.sampler_uniforms[index]];
        int unit = u.unit >= 0 && u.unit < 16 ? u.unit : -1;
        unsigned texture = unit >= 0 ? state.textures[unit] : 0;
        if (unit_version != texture_version) {
            unit_texture.fill(0);
            unit_image.fill(nullptr);
            unit_version = texture_version;
        }
        Image *im = unit >= 0 && unit_texture[unit] == texture ? unit_image[unit] : nullptr;
        if (!im) {
            auto found = textures.find(texture);
            im = found != textures.end() && found->second ? found->second.get() : white.get();
            if (unit >= 0) {
                unit_texture[unit] = texture;
                unit_image[unit] = im;
            }
        }
        if (im->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            if (pass_color || pass_depth)
                barrier();
            transition(*im, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        images[u.sampler] = {sampler(*im), im->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }
    /* Descriptor contents depend on textures and uniform range, not on the
       draw's uniform slice. Dynamic offsets keep each draw's values distinct. */
    std::array<uint64_t, 17> key{};
    key[0] = p.values.size();
    for (unsigned i = 0; i < sampler_count; i++) {
        key[1 + i * 2] = (uint64_t)images[i].sampler;
        key[2 + i * 2] = (uint64_t)images[i].imageView;
    }
    if (!uncached_descriptors) {
        if (last_set && last_set_key == key)
            return last_set;
        auto found = draw_sets.find(key);
        if (found != draw_sets.end()) {
            last_set = found->second;
            last_set_key = key;
            return found->second;
        }
    }
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = descriptor_pool;
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &descriptors;
    VkDescriptorSet set;
    check(vkAllocateDescriptorSets(device, &allocation, &set), "vkAllocateDescriptorSets");
    descriptor_count++;
    statistics.descriptor_builds++;
    VkDescriptorBufferInfo uniform_buffer{arena.handle, 0, p.values.size()};
    std::array<VkWriteDescriptorSet, 9> writes{};
    /* Unused shader bindings need no descriptor write. Course materials use
       only a subset of the shared layout's eight texture slots. */
    for (unsigned i = 0; i <= sampler_count; i++) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = i ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                     : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        if (i)
            writes[i].pImageInfo = &images[i - 1];
        else
            writes[i].pBufferInfo = &uniform_buffer;
    }
    vkUpdateDescriptorSets(device, sampler_count + 1, writes.data(), 0, nullptr);
    statistics.descriptor_updates++;
    if (!uncached_descriptors) {
        draw_sets.emplace(key, set);
        last_set = set;
        last_set_key = key;
    }
    return set;
}
VkFramebuffer Context::target_framebuffer(Image &color, Image *depth, VkRenderPass pass)
{
    std::array<VkImageView, 3> key = {color.attachment,
                                     depth ? depth->attachment : VK_NULL_HANDLE,
                                     (VkImageView)(uintptr_t)pass};
    if (!transient_framebuffers) {
        auto found = target_cache.find(key);
        if (found != target_cache.end())
            return found->second;
    }
    VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    info.renderPass = pass;
    info.attachmentCount = depth ? 2 : 1;
    info.pAttachments = key.data();
    info.width = color.width;
    info.height = color.height;
    info.layers = 1;
    VkFramebuffer framebuffer;
    check(vkCreateFramebuffer(device, &info, nullptr, &framebuffer), "vkCreateFramebuffer");
    statistics.framebuffer_creations++;
    if (transient_framebuffers)
        transient_targets.push_back(framebuffer);
    else
        target_cache.emplace(key, framebuffer);
    return framebuffer;
}
void Context::draw(GLenum topology, int first, int count)
{
    if (count < 1)
        return;
    switch (draw_source) {
    case 1: statistics.draw_track++; break;
    case 2: statistics.draw_car++; break;
    case 3: statistics.draw_shadow++; break;
    case 4: statistics.draw_ui++; break;
    default: statistics.draw_other++; break;
    }
    {
        auto &pp = programs[state.program];
        if (pp.dump_id >= 0 && pp.dump_id < 64) {
            statistics.program_draws[pp.dump_id]++;
            statistics.program_source[pp.dump_id][draw_source >= 0 && draw_source < 5
                                                     ? draw_source : 0]++;
        }
    }
    uint64_t prepare_start = profile_draws ? SDL_GetPerformanceCounter() : 0;
    auto &p = programs[state.program];
    if (!p.linked)
        return;
    auto *target = color(state.draw_fb);
    if (!target)
        return;
    auto *z = depth(state.draw_fb);
    auto &vao = arrays[state.vao];
    /* Upload immutable meshes before reserving this draw's arena slice:
       an upload may flush a full arena, invalidating earlier slice offsets. */
    std::array<const Buffer *, 16> resident{};
    for (int i = 0; i < 16; i++)
        if (p.inputs[i] && vao.attributes[i].enabled) {
            int shared = -1;
            for (int j = 0; j < i; j++)
                if (p.inputs[j] && vao.attributes[j].enabled &&
                    vao.attributes[j].buffer == vao.attributes[i].buffer) {
                    shared = j;
                    break;
                }
            resident[i] = shared < 0 ? vertex_buffer(vao.attributes[i].buffer) : resident[shared];
        }
    /* Resident bindings can retain their base offsets across material groups.
       Streaming buffers still contain only this draw's selected subrange. */
    bool base_vertices = !repeat_commands && !p.vertex_id;
    bool streamed = false, resident_draw = false;
    for (int i = 0; i < 16; i++)
        if (p.inputs[i] && vao.attributes[i].enabled) {
            if (resident[i])
                resident_draw = true;
            else {
                streamed = true;
                base_vertices = false;
            }
        }
    if (streamed)
        statistics.streamed_draws++;
    else if (resident_draw)
        statistics.resident_draws++;
    size_t bytes = p.values.size() + 512;
    std::array<size_t, 16> strides{};
    std::array<int, 16> shared;
    shared.fill(-1);
    for (int i = 0; i < 16; i++)
        if (p.inputs[i]) {
            auto &a = vao.attributes[i];
            size_t stride = a.stride ? a.stride : a.size * (a.type == GL_FLOAT ? 4 : 1);
            strides[i] = stride;
            for (int j = 0; j < i; j++)
                if (p.inputs[j] && vao.attributes[j].enabled &&
                    a.buffer == vao.attributes[j].buffer && stride == strides[j]) {
                    shared[i] = j;
                    break;
                }
            if (!a.enabled)
                bytes += 16;
            else if (!resident[i] && shared[i] < 0)
                bytes += (size_t)count * stride;
            bytes += 16;
        }
    size_t offset = reserve(
        bytes, std::max((size_t)16, (size_t)properties.limits.minUniformBufferOffsetAlignment));
    statistics.update_buffer++;
    statistics.uniform_upload_bytes += p.values.size();
    memcpy((uint8_t *)arena.mapped + offset, p.values.data(), p.values.size());
    size_t vertex_offset = offset + aligned((int)p.values.size(), 16);
    std::array<VkDeviceSize, 16> offsets{};
    std::array<VkBuffer, 16> vertex_handles{};
    vertex_handles.fill(arena.handle);
    for (unsigned i = 0; i < 16; i++)
        if (p.inputs[i]) {
            auto &a = vao.attributes[i];
            offsets[i] = vertex_offset;
            if (a.enabled) {
                size_t stride = a.stride ? a.stride : a.size * (a.type == GL_FLOAT ? 4 : 1);
                auto &source = buffers[a.buffer];
                size_t start = first * stride;
                if (start > source.size() ||
                    a.offset + (count - 1) * stride + a.size * (a.type == GL_FLOAT ? 4 : 1) >
                        source.size() - start)
                    throw std::runtime_error("Vulkan vertex attribute bounds");
                if (resident[i]) {
                    vertex_handles[i] = resident[i]->handle;
                    offsets[i] = (base_vertices ? 0 : start) + a.offset;
                    continue;
                }
                if (shared[i] >= 0) {
                    unsigned previous = (unsigned)shared[i];
                    offsets[i] = offsets[previous] - vao.attributes[previous].offset + a.offset;
                } else {
                    size_t size = std::min((size_t)count * stride,
                                           source.size() - std::min(start, source.size()));
                    if (start > source.size())
                        throw std::runtime_error("Vulkan vertex buffer bounds");
                    statistics.update_buffer++;
                    memcpy((uint8_t *)arena.mapped + vertex_offset, source.data() + start, size);
                    statistics.vertex_upload_bytes += size;
                    offsets[i] = vertex_offset + a.offset;
                    vertex_offset += aligned((int)size, 16);
                }
            } else {
                // Disabled inputs share the same immutable default in this
                // command arena. A pool reset invalidates the cached offset.
                if (repeat_commands || default_vertex_offset == SIZE_MAX) {
                    float value[4] = {0, 0, 0, 1};
                    statistics.update_buffer++;
                    memcpy((uint8_t *)arena.mapped + vertex_offset, value, sizeof value);
                    default_vertex_offset = vertex_offset;
                    vertex_offset += sizeof value;
                }
                offsets[i] = default_vertex_offset;
            }
        }
    uint64_t descriptors_start = profile_draws ? SDL_GetPerformanceCounter() : 0;
    VkDescriptorSet set = draw_descriptors(p);
    uint64_t pipeline_start = profile_draws ? SDL_GetPerformanceCounter() : 0;
    VkRenderPass pass = render_pass(target->format, z ? z->format : VK_FORMAT_UNDEFINED);
    VkImageView views[] = {target->attachment, z ? z->attachment : VK_NULL_HANDLE};
    /* Attachment ordering and blending are already ordered within a subpass.
       Keep compatible draws together instead of draining the GPU between
       every course material. Transfers and clears end the pass via barrier(). */
    if (pass_color != views[0] || pass_depth != views[1]) {
        barrier();
        transition(*target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        if (z)
            transition(*z, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        VkFramebuffer fb = target_framebuffer(*target, z, pass);
        last_pass = pass;
        last_framebuffer = fb;
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = pass;
        begin.framebuffer = fb;
        begin.renderArea.extent = {(uint32_t)target->width, (uint32_t)target->height};
        if (pass_queries)
            pass_mark("pass " + std::to_string(target->width) + "x" + std::to_string(target->height) +
                      " f" + std::to_string(target->format) + (z ? " d" + std::to_string(z->format) : "") +
                      " p" + std::to_string(state.program));
        vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
        statistics.passes++;
        pass_color = views[0];
        pass_depth = views[1];
    }
    last_pass = pass;
    VkPipeline selected_pipeline = pipeline(*this, p, pass, topology);
    uint64_t commands_start = profile_draws ? SDL_GetPerformanceCounter() : 0;
    VkViewport viewport{(float)state.viewport[0],
                        (float)state.viewport[1],
                        (float)state.viewport[2],
                        (float)state.viewport[3],
                        0,
                        1};
    VkRect2D clip{{0, 0}, {(uint32_t)target->width, (uint32_t)target->height}};
    if (state.clip) {
        int x0 = std::max(0, state.scissor[0]), y0 = std::max(0, state.scissor[1]);
        int x1 = std::min(target->width, state.scissor[0] + state.scissor[2]),
            y1 = std::min(target->height, state.scissor[1] + state.scissor[3]);
        clip = {{x0, y0}, {(uint32_t)std::max(0, x1 - x0), (uint32_t)std::max(0, y1 - y0)}};
    }
    uint32_t dynamic_offset = (uint32_t)offset;
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, p.layout, 0, 1, &set, 1,
                            &dynamic_offset);
    unsigned binding_count = 0;
    for (unsigned i = 0; i < 16; i++)
        if (p.inputs[i])
            binding_count = i + 1;
    bind_draw_state(selected_pipeline, viewport, clip, binding_count, vertex_handles, offsets);
    vkCmdDraw(command, count, 1, base_vertices ? first : 0, 0);
    if (!pass_draws.empty())
        pass_draws.back()++;
    if (profile_draws) {
        uint64_t end = SDL_GetPerformanceCounter(), frequency = SDL_GetPerformanceFrequency();
        statistics.cpu_prepare_ns += (descriptors_start - prepare_start) * 1000000000ull / frequency;
        statistics.cpu_descriptors_ns += (pipeline_start - descriptors_start) * 1000000000ull / frequency;
        statistics.cpu_pipeline_ns += (commands_start - pipeline_start) * 1000000000ull / frequency;
        statistics.cpu_commands_ns += (end - commands_start) * 1000000000ull / frequency;
    }
    statistics.draws++;
}
void uniform(int location, int count, int components, const void *values, bool matrix)
{
    auto &p = context().programs[context().state.program];
    if (location < 0 || location >= (int)p.uniforms.size())
        return;
    auto &u = p.uniforms[location];
    if (u.sampler >= 0) {
        u.unit = *(const int *)values;
        return;
    }
    for (int i = 0; i < std::min(count, u.count); i++) {
        auto *out = p.values.data() + u.offset + i * u.stride;
        if (matrix)
            for (int col = 0; col < components; col++)
                memcpy(out + col * 16,
                       (const uint8_t *)values +
                           (i * components * components + col * components) * 4,
                       components * 4);
        else
            memcpy(out, (const uint8_t *)values + i * components * 4, components * 4);
    }
}
} // namespace moon
