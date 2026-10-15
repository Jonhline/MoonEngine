/* Adapt actual native SDK states to equivalent host GL calls for comparison.
   Keep the SDK in C++: its vector extensions are not accepted by MSVC C. */
#include "../../src/platform/switch/deko_blend.h"
#include "../../src/platform/switch/deko_blend_commands.h"
#include <SDL_opengl.h>
#include <cstring>

namespace {
uint32_t packet[8];
bool packet_valid;
} // namespace
extern "C" void dkCmdBufReplayCmds(DkCmdBuf, const uint32_t *words, uint32_t count)
{
    packet_valid = count == 8;
    if (packet_valid)
        std::memcpy(packet, words, sizeof packet);
}
static GLenum factor(uint32_t value)
{
    /* Decode the actual Maxwell command data, not the source SDK struct.
       NVIDIA clb197.h D3D ZERO..INVDESTALPHA / OGL CONSTANT_ALPHA. */
    switch (value) {
    case 1:
        return GL_ZERO;
    case 2:
        return GL_ONE;
    case 5:
        return GL_SRC_ALPHA;
    case 6:
        return GL_ONE_MINUS_SRC_ALPHA;
    case 7:
        return GL_DST_ALPHA;
    case 8:
        return GL_ONE_MINUS_DST_ALPHA;
    case 0xc003:
        return GL_CONSTANT_ALPHA;
    case 0xc004:
        return GL_ONE_MINUS_CONSTANT_ALPHA;
    default:
        return (GLenum)-1;
    }
}
static GLenum operation(uint32_t value)
{
    return value == 2 ? GL_FUNC_SUBTRACT : value == 3 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
}
extern "C" int deko_blend_state(unsigned enabled, unsigned equation, int negative,
                                unsigned values[7])
{
    DkColorState color;
    DkBlendState blend;
    nd_material_blend(enabled != 0, equation, &color, &blend);
    /* Restore the old native 1+C -> C mistake. */
    if (negative == 1 && (equation & 3) == 0 && ((equation >> 2) & 3) != 0 &&
        ((equation >> 6) & 3) == 0) {
        unsigned c = (equation >> 4) & 3;
        blend.srcColorBlendFactor = c == 0   ? DkBlendFactor_SrcAlpha
                                    : c == 1 ? DkBlendFactor_DstAlpha
                                             : DkBlendFactor_ConstAlpha;
    }
    packet_valid = false;
    nd_bind_blend_commands(nullptr, &blend);
    if (negative == 2)
        packet[1] = 0; /* Missing per-target separate-alpha enable. */
    if (negative == 3)
        packet[7] = packet[4]; /* SDK 0.5 writes the RGB destination here. */
    /* Increasing packet, 7 methods on subchannel 0, byte address 1E00. */
    if (!packet_valid || packet[0] != 0x20070780u || packet[1] != 1)
        return 0;
    values[0] = color.blendEnableMask;
    values[1] = operation(packet[2]);
    values[2] = operation(packet[5]);
    values[3] = factor(packet[3]);
    values[4] = factor(packet[4]);
    values[5] = factor(packet[6]);
    values[6] = factor(packet[7]);
    return 1;
}
