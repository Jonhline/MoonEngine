#include "vk_internal.hpp"

namespace moon {
void Context::bind_draw_state(VkPipeline selected, const VkViewport &viewport, const VkRect2D &clip,
                              unsigned bindings, const std::array<VkBuffer, 16> &vertices,
                              const std::array<VkDeviceSize, 16> &offsets)
{
    // Command-buffer state survives render-pass boundaries, but never a reset.
    if (repeat_commands || emitted.pipeline != selected) {
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, selected);
        statistics.vk_bind_pipeline++;
        emitted.pipeline = selected;
        statistics.pipeline_binds++;
    }
    if (repeat_commands || !emitted.dynamic_valid ||
        memcmp(&emitted.viewport, &viewport, sizeof viewport)) {
        vkCmdSetViewport(command, 0, 1, &viewport);
        statistics.vk_set_viewport++;
        emitted.viewport = viewport;
        statistics.dynamic_sets++;
    }
    if (repeat_commands || !emitted.dynamic_valid || memcmp(&emitted.scissor, &clip, sizeof clip)) {
        vkCmdSetScissor(command, 0, 1, &clip);
        statistics.vk_set_scissor++;
        emitted.scissor = clip;
        statistics.dynamic_sets++;
    }
    if (repeat_commands || !emitted.dynamic_valid ||
        memcmp(emitted.blend.data(), state.blend_color, sizeof state.blend_color)) {
        vkCmdSetBlendConstants(command, state.blend_color);
        memcpy(emitted.blend.data(), state.blend_color, sizeof state.blend_color);
        statistics.dynamic_sets++;
    }
    emitted.dynamic_valid = true;
    if (bindings &&
        (repeat_commands || emitted.bindings != bindings ||
         !std::equal(vertices.begin(), vertices.begin() + bindings, emitted.vertices.begin()) ||
         !std::equal(offsets.begin(), offsets.begin() + bindings, emitted.offsets.begin()))) {
        vkCmdBindVertexBuffers(command, 0, bindings, vertices.data(), offsets.data());
        statistics.vk_bind_vertices++;
        emitted.vertices = vertices;
        emitted.offsets = offsets;
        emitted.bindings = bindings;
        statistics.vertex_binds++;
    }
}
} // namespace moon
