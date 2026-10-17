#include "vk_internal.hpp"
#include <cstdio>
#include <filesystem>

namespace moon {
static constexpr uint64_t recipe_magic = 0x315045434552564dull;
static constexpr size_t recipe_limit = 4096;
static uint64_t checksum(const void *data, size_t size, uint64_t hash = 14695981039346656037ull)
{
    const auto *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; i++)
        hash = (hash ^ bytes[i]) * 1099511628211ull;
    return hash;
}
uint64_t shader_fingerprint(const Program &program)
{
    uint64_t hash = checksum(program.vertex.c_str(), program.vertex.size() + 1);
    return checksum(program.fragment.data(), program.fragment.size(), hash);
}
void Context::read_pipeline_recipes()
{
    if (pipeline_cache_path.empty() || getenv("MOON_VK_SKIP_VARIANTS"))
        return;
    std::string path = pipeline_cache_path + ".variants";
    SDL_RWops *file = SDL_RWFromFile(path.c_str(), "rb");
    if (file) {
        std::array<uint64_t, 5> header{};
        Sint64 bytes = SDL_RWsize(file);
        if (SDL_RWread(file, header.data(), sizeof header, 1) == 1 && header[0] == recipe_magic &&
            header[1] == 1 && header[2] == sizeof(PipelineRecipe) && header[3] <= recipe_limit &&
            bytes == (Sint64)(sizeof header + header[3] * sizeof(PipelineRecipe))) {
            std::vector<PipelineRecipe> values((size_t)header[3]);
            if (SDL_RWread(file, values.data(), sizeof(PipelineRecipe), values.size()) ==
                    values.size() &&
                checksum(values.data(), values.size() * sizeof(PipelineRecipe)) == header[4])
                pipeline_recipes.insert(values.begin(), values.end());
        }
        SDL_RWclose(file);
    }
    fprintf(stderr, "Vulkan variants: loaded %zu recipes\n", pipeline_recipes.size());
}
void Context::save_pipeline_recipes()
{
    if (pipeline_cache_path.empty() || getenv("MOON_VK_SKIP_VARIANTS"))
        return;
    std::vector<PipelineRecipe> values(pipeline_recipes.begin(), pipeline_recipes.end());
    std::array<uint64_t, 5> header = {
        recipe_magic, 1, sizeof(PipelineRecipe), values.size(),
        checksum(values.data(), values.size() * sizeof(PipelineRecipe))};
    std::string path = pipeline_cache_path + ".variants";
    std::string temporary = path + "." + std::to_string(SDL_GetPerformanceCounter()) + ".tmp";
    SDL_RWops *file = SDL_RWFromFile(temporary.c_str(), "wb");
    if (!file)
        return;
    bool written =
        SDL_RWwrite(file, header.data(), sizeof header, 1) == 1 &&
        SDL_RWwrite(file, values.data(), sizeof(PipelineRecipe), values.size()) == values.size();
    written = SDL_RWclose(file) == 0 && written;
    std::error_code error;
    if (written) {
        std::filesystem::remove(std::filesystem::u8path(path), error);
        error.clear();
        std::filesystem::rename(std::filesystem::u8path(temporary), std::filesystem::u8path(path),
                                error);
    }
    std::filesystem::remove(std::filesystem::u8path(temporary), error);
}
void Context::remember_pipeline(const Program &program, VkRenderPass pass, GLenum topology)
{
    if (warming_recipes || pipeline_recipes.size() >= recipe_limit || pipeline_cache_path.empty() ||
        getenv("MOON_VK_SKIP_VARIANTS"))
        return;
    PipelineRecipe recipe{};
    recipe[0] = program.fingerprint;
    for (const auto &entry : passes)
        if (entry.second == pass) {
            recipe[1] = entry.first[0];
            recipe[2] = entry.first[1];
            break;
        }
    recipe[3] = topology;
    const auto &s = state;
    const uint64_t fields[] = {
        s.blend,          s.depth,         s.cull,    s.depth_write,    s.depth_func,
        s.cull_face,      s.front_face,    s.src_rgb, s.dst_rgb,        s.src_a,
        s.dst_a,          s.eq_rgb,        s.eq_a,    s.color_write[0], s.color_write[1],
        s.color_write[2], s.color_write[3]};
    std::copy(std::begin(fields), std::end(fields), recipe.begin() + 4);
    const auto &vao = arrays[s.vao];
    for (unsigned i = 0; i < 16; i++) {
        const auto &attribute = vao.attributes[i];
        const uint64_t data[] = {(uint64_t)attribute.size, (uint64_t)attribute.stride,
                                 attribute.type,           attribute.normalized,
                                 attribute.integer,        attribute.enabled};
        std::copy(std::begin(data), std::end(data), recipe.begin() + 21 + i * 6);
    }
    pipeline_recipes.insert(recipe);
}
static void restore_recipe(State &s, VertexArray &vao, const PipelineRecipe &recipe)
{
    s.blend = recipe[4] != 0;
    s.depth = recipe[5] != 0;
    s.cull = recipe[6] != 0;
    s.depth_write = recipe[7] != 0;
    s.depth_func = (GLenum)recipe[8];
    s.cull_face = (GLenum)recipe[9];
    s.front_face = (GLenum)recipe[10];
    s.src_rgb = (GLenum)recipe[11];
    s.dst_rgb = (GLenum)recipe[12];
    s.src_a = (GLenum)recipe[13];
    s.dst_a = (GLenum)recipe[14];
    s.eq_rgb = (GLenum)recipe[15];
    s.eq_a = (GLenum)recipe[16];
    for (unsigned i = 0; i < 4; i++)
        s.color_write[i] = recipe[17 + i] != 0;
    for (unsigned i = 0; i < 16; i++) {
        auto &a = vao.attributes[i];
        unsigned base = 21 + i * 6;
        a.size = (int)recipe[base];
        a.stride = (int)recipe[base + 1];
        a.type = (GLenum)recipe[base + 2];
        a.normalized = recipe[base + 3] != 0;
        a.integer = recipe[base + 4] != 0;
        a.enabled = recipe[base + 5] != 0;
    }
}
void Context::warm_pipeline_recipes(Program &program)
{
    if (pipeline_recipes.empty() || getenv("MOON_VK_SKIP_VARIANTS"))
        return;
    // Only pipeline descriptions are reconstructed. No draw, buffer upload,
    // camera update or game-state mutation is performed during this warm-up.
    State saved = state;
    unsigned scratch = next_id++;
    state.vao = scratch;
    auto &vao = arrays[scratch];
    warming_recipes = true;
    size_t before = program.pipelines.size();
    for (const auto &recipe : pipeline_recipes) {
        if (recipe[0] != program.fingerprint)
            continue;
        try {
            restore_recipe(state, vao, recipe);
            VkRenderPass pass = render_pass((VkFormat)recipe[1], (VkFormat)recipe[2]);
            pipeline(*this, program, pass, (GLenum)recipe[3]);
        } catch (const std::exception &error) {
            fprintf(stderr, "Vulkan variant preparation skipped: %s\n", error.what());
        }
    }
    state = saved;
    arrays.erase(scratch);
    warming_recipes = false;
    size_t prepared = program.pipelines.size() - before;
    statistics.pipeline_warmups += prepared;
    if (prepared)
        fprintf(stderr, "Vulkan variants: prepared %zu pipelines\n", prepared);
}
} // namespace moon
