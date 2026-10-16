/* Native Vulkan 1.0 device and transfer path; no game data or window required.
   Function pointers belong to the context so teardown also works after a partial init. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL.h>
#include "vulkan_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEVICE_FUNCTIONS(X)                                                                        \
    X(DestroyDevice)                                                                               \
    X(GetDeviceQueue) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements)              \
        X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory)            \
            X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers)                   \
                X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdFillBuffer) X(CmdPipelineBarrier)   \
                    X(CreateFence) X(DestroyFence) X(QueueSubmit) X(WaitForFences)                 \
                        X(DeviceWaitIdle)

struct moon_vk_device {
    void *library;
    PFN_vkGetInstanceProcAddr get_proc;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkCreateDevice CreateDevice;
#define DECLARE(name) PFN_vk##name name;
    DEVICE_FUNCTIONS(DECLARE)
#undef DECLARE
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;
    uint32_t family;
    char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
};

static int failure(char *error, size_t capacity, const char *operation, VkResult result)
{
    snprintf(error, capacity, "%s (Vulkan result %d)", operation, (int)result);
    return 0;
}

void moon_vk_close(moon_vk_device *v)
{
    if (!v)
        return;
    if (v->device && v->DestroyDevice) {
        if (v->DeviceWaitIdle)
            v->DeviceWaitIdle(v->device);
        v->DestroyDevice(v->device, NULL);
    }
    if (v->instance && v->DestroyInstance)
        v->DestroyInstance(v->instance, NULL);
    if (v->library)
        SDL_UnloadObject(v->library);
    free(v);
}

static int choose_device(moon_vk_device *v)
{
    uint32_t count = 0;
    if (v->EnumeratePhysicalDevices(v->instance, &count, NULL) != VK_SUCCESS || !count)
        return 0;
    VkPhysicalDevice *devices = calloc(count, sizeof *devices);
    if (!devices)
        return 0;
    VkResult result = v->EnumeratePhysicalDevices(v->instance, &count, devices);
    for (uint32_t i = 0; result == VK_SUCCESS && i < count && !v->physical; i++) {
        uint32_t families = 0;
        v->GetPhysicalDeviceQueueFamilyProperties(devices[i], &families, NULL);
        VkQueueFamilyProperties *properties = calloc(families, sizeof *properties);
        if (!properties)
            continue;
        v->GetPhysicalDeviceQueueFamilyProperties(devices[i], &families, properties);
        for (uint32_t j = 0; j < families; j++) {
            if (properties[j].queueCount && (properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                v->physical = devices[i];
                v->family = j;
                break;
            }
        }
        free(properties);
    }
    free(devices);
    if (!v->physical)
        return 0;
    VkPhysicalDeviceProperties properties;
    v->GetPhysicalDeviceProperties(v->physical, &properties);
    snprintf(v->name, sizeof v->name, "%s", properties.deviceName);
    return 1;
}

static int open_device(moon_vk_device *v, char *error, size_t capacity)
{
    float priority = 1.f;
    VkDeviceQueueCreateInfo queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = v->family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue;
    VkResult result = v->CreateDevice(v->physical, &info, NULL, &v->device);
    if (result != VK_SUCCESS)
        return failure(error, capacity, "vkCreateDevice", result);
    /* Load destruction first to keep subsequent lookup failures recoverable. */
#define LOAD(name)                                                                                 \
    v->name = (PFN_vk##name)v->get_proc(v->instance, "vk" #name);                                  \
    if (!v->name) {                                                                                \
        snprintf(error, capacity, "Missing Vulkan entry point: vk%s", #name);                      \
        return 0;                                                                                  \
    }
    DEVICE_FUNCTIONS(LOAD)
#undef LOAD
    v->GetDeviceQueue(v->device, v->family, 0, &v->queue);
    return 1;
}

moon_vk_device *moon_vk_open(const char *loader, char *error, size_t capacity)
{
    if (!error || !capacity)
        return NULL;
    error[0] = 0;
    moon_vk_device *v = calloc(1, sizeof *v);
    if (!v) {
        snprintf(error, capacity, "Could not allocate Vulkan context");
        return NULL;
    }
#ifdef _WIN32
    const char *default_loader = "vulkan-1.dll";
#else
    const char *default_loader = "libvulkan.so.1";
#endif
    v->library = SDL_LoadObject(loader ? loader : default_loader);
    if (!v->library) {
        snprintf(error, capacity, "Vulkan loader: %.180s", SDL_GetError());
        goto failed;
    }
    v->get_proc = (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(v->library, "vkGetInstanceProcAddr");
    PFN_vkCreateInstance create =
        v->get_proc ? (PFN_vkCreateInstance)v->get_proc(VK_NULL_HANDLE, "vkCreateInstance") : NULL;
    if (!create) {
        snprintf(error, capacity, "Missing Vulkan instance entry point");
        goto failed;
    }
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Moon Engine";
    app.pEngineName = "Moon Engine";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    VkResult result = create(&info, NULL, &v->instance);
    if (result != VK_SUCCESS) {
        failure(error, capacity, "vkCreateInstance", result);
        goto failed;
    }
#define INSTANCE(name)                                                                             \
    v->name = (PFN_vk##name)v->get_proc(v->instance, "vk" #name);                                  \
    if (!v->name) {                                                                                \
        snprintf(error, capacity, "Missing Vulkan entry point: vk%s", #name);                      \
        goto failed;                                                                               \
    }
    INSTANCE(DestroyInstance)
    INSTANCE(EnumeratePhysicalDevices)
    INSTANCE(GetPhysicalDeviceProperties)
    INSTANCE(GetPhysicalDeviceQueueFamilyProperties)
    INSTANCE(GetPhysicalDeviceMemoryProperties)
    INSTANCE(CreateDevice)
#undef INSTANCE
    if (!choose_device(v)) {
        snprintf(error, capacity, "No Vulkan device with a graphics queue");
        goto failed;
    }
    if (open_device(v, error, capacity))
        return v;
failed:
    moon_vk_close(v);
    return NULL;
}

const char *moon_vk_name(const moon_vk_device *v)
{
    return v ? v->name : "";
}

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkCommandPool pool;
    VkCommandBuffer command;
    VkFence fence;
} transfer;

static int allocate_buffer(moon_vk_device *v, transfer *t, char *error, size_t capacity)
{
    VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = 4096;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkResult result = v->CreateBuffer(v->device, &info, NULL, &t->buffer);
    if (result != VK_SUCCESS)
        return failure(error, capacity, "vkCreateBuffer", result);
    VkMemoryRequirements requirements;
    v->GetBufferMemoryRequirements(v->device, t->buffer, &requirements);
    VkPhysicalDeviceMemoryProperties properties;
    v->GetPhysicalDeviceMemoryProperties(v->physical, &properties);
    uint32_t type;
    VkMemoryPropertyFlags flags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (type = 0; type < properties.memoryTypeCount; type++)
        if ((requirements.memoryTypeBits & (1u << type)) &&
            (properties.memoryTypes[type].propertyFlags & flags) == flags)
            break;
    if (type == properties.memoryTypeCount) {
        snprintf(error, capacity, "No host-visible coherent Vulkan memory");
        return 0;
    }
    VkMemoryAllocateInfo allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = type;
    result = v->AllocateMemory(v->device, &allocation, NULL, &t->memory);
    if (result != VK_SUCCESS)
        return failure(error, capacity, "vkAllocateMemory", result);
    result = v->BindBufferMemory(v->device, t->buffer, t->memory, 0);
    return result == VK_SUCCESS || failure(error, capacity, "vkBindBufferMemory", result);
}

static int record_fill(moon_vk_device *v, transfer *t, uint32_t pattern, char *error,
                       size_t capacity)
{
    VkCommandPoolCreateInfo pool = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.queueFamilyIndex = v->family;
    VkResult result = v->CreateCommandPool(v->device, &pool, NULL, &t->pool);
    if (result != VK_SUCCESS)
        return failure(error, capacity, "vkCreateCommandPool", result);
    VkCommandBufferAllocateInfo allocation = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocation.commandPool = t->pool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1;
    result = v->AllocateCommandBuffers(v->device, &allocation, &t->command);
    if (result != VK_SUCCESS)
        return failure(error, capacity, "vkAllocateCommandBuffers", result);
    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = v->BeginCommandBuffer(t->command, &begin);
    if (result != VK_SUCCESS)
        return failure(error, capacity, "vkBeginCommandBuffer", result);
    v->CmdFillBuffer(t->command, t->buffer, 0, 4096, pattern);
    VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    v->CmdPipelineBarrier(t->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                          1, &barrier, 0, NULL, 0, NULL);
    result = v->EndCommandBuffer(t->command);
    return result == VK_SUCCESS || failure(error, capacity, "vkEndCommandBuffer", result);
}

int moon_vk_check_transfer(moon_vk_device *v, uint32_t pattern, uint32_t expected, char *error,
                           size_t capacity)
{
    if (!v || !error || !capacity)
        return 0;
    transfer t = {0};
    int ok = 0;
    error[0] = 0;
    if (!allocate_buffer(v, &t, error, capacity) || !record_fill(v, &t, pattern, error, capacity))
        goto done;
    VkFenceCreateInfo fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkResult result = v->CreateFence(v->device, &fence, NULL, &t.fence);
    if (result != VK_SUCCESS) {
        failure(error, capacity, "vkCreateFence", result);
        goto done;
    }
    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &t.command;
    result = v->QueueSubmit(v->queue, 1, &submit, t.fence);
    if (result == VK_SUCCESS)
        result = v->WaitForFences(v->device, 1, &t.fence, VK_TRUE, UINT64_C(5000000000));
    if (result != VK_SUCCESS) {
        failure(error, capacity, "Vulkan transfer submission/wait", result);
        /* A timeout does not release resources that might still be in flight. */
        v->DeviceWaitIdle(v->device);
        goto done;
    }
    uint32_t *words = NULL;
    result = v->MapMemory(v->device, t.memory, 0, 4096, 0, (void **)&words);
    if (result != VK_SUCCESS) {
        failure(error, capacity, "vkMapMemory", result);
        goto done;
    }
    ok = 1;
    for (size_t i = 0; i < 1024; i++) {
        if (words[i] != expected) {
            snprintf(error, capacity, "Vulkan readback word %zu: %08x != %08x", i, words[i],
                     expected);
            ok = 0;
            break;
        }
    }
    v->UnmapMemory(v->device, t.memory);
done:
    if (t.fence)
        v->DestroyFence(v->device, t.fence, NULL);
    if (t.pool)
        v->DestroyCommandPool(v->device, t.pool, NULL);
    if (t.buffer)
        v->DestroyBuffer(v->device, t.buffer, NULL);
    if (t.memory)
        v->FreeMemory(v->device, t.memory, NULL);
    return ok;
}
