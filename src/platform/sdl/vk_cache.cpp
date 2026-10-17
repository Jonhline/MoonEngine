#include "vk_internal.hpp"
#include <filesystem>
#include <cstdio>

namespace moon {
void Context::open_pipeline_cache()
{
    if (getenv("MOON_VK_NO_PIPELINE_CACHE"))
        return;
    const char *override_path = getenv("MOON_VK_CACHE_DIR");
    char *preferences = override_path ? nullptr : SDL_GetPrefPath("MoonEngine", "MoonEngine");
    std::filesystem::path directory = std::filesystem::u8path(override_path ? override_path
                                                              : preferences ? preferences
                                                                            : "");
    SDL_free(preferences);
    std::error_code error;
    if (!directory.empty()) {
        directory /= "vulkan";
        std::filesystem::create_directories(directory, error);
        char name[100];
        snprintf(name, sizeof name, "pipelines-%08x-%08x-%08x.bin", properties.vendorID,
                 properties.deviceID, properties.driverVersion);
        if (!error)
            pipeline_cache_path = (directory / name).u8string();
    }
    std::vector<uint8_t> bytes;
    SDL_RWops *file =
        pipeline_cache_path.empty() ? nullptr : SDL_RWFromFile(pipeline_cache_path.c_str(), "rb");
    if (file) {
        Sint64 size = SDL_RWsize(file);
        if (size >= 32 && size <= 64 * 1024 * 1024) {
            bytes.resize((size_t)size);
            if (SDL_RWread(file, bytes.data(), bytes.size(), 1) != 1)
                bytes.clear();
        }
        SDL_RWclose(file);
    }
    if (!bytes.empty()) {
        uint32_t header[4];
        memcpy(header, bytes.data(), sizeof header);
        if (header[0] < 32 || header[0] > bytes.size() || header[1] != 1 ||
            header[2] != properties.vendorID || header[3] != properties.deviceID ||
            memcmp(bytes.data() + 16, properties.pipelineCacheUUID, VK_UUID_SIZE))
            bytes.clear();
    }
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    info.initialDataSize = bytes.size();
    info.pInitialData = bytes.empty() ? nullptr : bytes.data();
    VkResult result = vkCreatePipelineCache(device, &info, nullptr, &pipeline_cache);
    if (result != VK_SUCCESS && !bytes.empty()) {
        bytes.clear();
        info.initialDataSize = 0;
        info.pInitialData = nullptr;
        result = vkCreatePipelineCache(device, &info, nullptr, &pipeline_cache);
    }
    check(result, "pipeline cache");
    read_pipeline_recipes();
    statistics.pipeline_cache_bytes = bytes.size();
    fprintf(stderr, "Vulkan pipeline cache: loaded %zu bytes\n", bytes.size());
}
void Context::save_pipeline_cache()
{
    save_pipeline_recipes();
    if (!pipeline_cache || pipeline_cache_path.empty())
        return;
    size_t size = 0;
    if (vkGetPipelineCacheData(device, pipeline_cache, &size, nullptr) != VK_SUCCESS ||
        size > 64 * 1024 * 1024)
        return;
    std::vector<uint8_t> bytes(size);
    if (vkGetPipelineCacheData(device, pipeline_cache, &size, bytes.data()) != VK_SUCCESS)
        return;
    std::string temporary =
        pipeline_cache_path + "." + std::to_string(SDL_GetPerformanceCounter()) + ".tmp";
    SDL_RWops *file = SDL_RWFromFile(temporary.c_str(), "wb");
    if (!file)
        return;
    bool written = SDL_RWwrite(file, bytes.data(), size, 1) == 1;
    written = SDL_RWclose(file) == 0 && written;
    std::error_code error;
    if (written) {
        std::filesystem::remove(std::filesystem::u8path(pipeline_cache_path), error);
        error.clear();
        std::filesystem::rename(std::filesystem::u8path(temporary),
                                std::filesystem::u8path(pipeline_cache_path), error);
    }
    std::filesystem::remove(std::filesystem::u8path(temporary), error);
}
} // namespace moon
