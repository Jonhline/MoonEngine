/* Execute the native transfer helpers with delayed 2D/copy engines.
   The SDK header supplies the real command signatures; no console binaries
   or GPU implementation are linked into this host scheduling fixture. */
#include "../../src/platform/switch/deko_transfer.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

namespace {
using Pixels = std::array<unsigned, 16>;
struct Image {
    DkImageView view{};
    Pixels memory{}, cached{};
    bool cache_valid{};
} source, mask;
Pixels upload;
std::vector<std::function<void()>> graphics, transfers;
unsigned barrier_count, observed_reads, copies;
bool failed;
const char *negative;
void drain(std::vector<std::function<void()>> &work)
{
    for (auto &run : work)
        run();
    work.clear();
}
Pixels sample(Image &image)
{
    if (!image.cache_valid) {
        image.cached = image.memory;
        image.cache_valid = true;
    }
    return image.cached;
}
void copy(const DkImageView *src, const DkImageRect *from, const DkImageView *dst,
          const DkImageRect *to)
{
    failed |= src != &source.view || dst != &mask.view;
    failed |= from->width != to->width || from->height != to->height;
    failed |= from->x + from->width > 4 || from->y + from->height > 4;
    failed |= to->x + to->width > 4 || to->y + to->height > 4;
    DkImageRect a = *from, b = *to;
    if (!std::strcmp(negative, "--snapshot-negative") && from->x == 1)
        b.x = 0; /* A translated snapshot must fail the untouched-pixel check. */
    copies++;
    transfers.push_back([a, b] {
        for (unsigned y = 0; y < a.height; ++y)
            for (unsigned x = 0; x < a.width; ++x)
                mask.memory[(b.y + y) * 4 + b.x + x] = source.memory[(a.y + y) * 4 + a.x + x];
    });
}
} // namespace
extern "C" void dkCmdBufBarrier(DkCmdBuf, DkBarrier mode, uint32_t flags)
{
    bool begin = (barrier_count++ & 1) == 0;
    if ((begin && !std::strcmp(negative, "--before-negative")) ||
        (!begin && !std::strcmp(negative, "--after-negative")))
        mode = DkBarrier_None;
    if (!std::strcmp(negative, "--cache-negative"))
        flags &= ~DkInvalidateFlags_Image;
    if (mode == DkBarrier_Full) {
        /* Legal hostile schedule: independent transfers outrun pending 3D.
           Correct pre-transfer ordering makes that pending work empty. */
        drain(transfers);
        drain(graphics);
    } else if (mode == DkBarrier_Fragments)
        drain(graphics);
    if (flags & DkInvalidateFlags_Image)
        source.cache_valid = mask.cache_valid = false;
}
extern "C" void dkCmdBufBlitImage(DkCmdBuf, const DkImageView *src, const DkImageRect *from,
                                  const DkImageView *dst, const DkImageRect *to, uint32_t flags,
                                  uint32_t factor)
{
    failed |= flags != DkBlitFlag_FilterLinear || factor != 0;
    copy(src, from, dst, to);
}
extern "C" void dkCmdBufCopyImage(DkCmdBuf, const DkImageView *src, const DkImageRect *from,
                                  const DkImageView *dst, const DkImageRect *to, uint32_t flags)
{
    failed |= flags != 0;
    copy(src, from, dst, to);
}
extern "C" void dkCmdBufCopyBufferToImage(DkCmdBuf, const DkCopyBuf *src, const DkImageView *dst,
                                          const DkImageRect *rect, uint32_t flags)
{
    failed |= src->addr != 123 || dst != &mask.view || flags != 0;
    failed |= rect->width != 4 || rect->height != 4;
    copies++;
    transfers.push_back([] { mask.memory = upload; });
}
int main(int argc, char **argv)
{
    negative = argc > 1 ? argv[1] : "";
    DkImageRect rect{0, 0, 0, 4, 4, 1};
    DkCopyBuf buffer{123, 0, 0};
    for (unsigned frame = 0; frame < 120; frame++) {
        Pixels old = sample(mask), next;
        for (unsigned i = 0; i < next.size(); i++)
            next[i] = (frame + 1) * 101 + i * 19;
        /* A prior fragment reads this same reusable DATE/particle image;
           another prior draw produces the source for the next snapshot. */
        graphics.push_back([old] {
            failed |= sample(mask) != old;
            observed_reads++;
        });
        graphics.push_back([next] { source.memory = next; });
        upload = next;
        if (frame % 3 == 0)
            nd_blit_commands(nullptr, &source.view, &rect, &mask.view, &rect);
        else if (frame % 3 == 1)
            nd_copy_commands(nullptr, &source.view, &rect, &mask.view, &rect);
        else
            nd_upload_commands(nullptr, &buffer, &mask.view, &rect);
        if (sample(mask) != next || !transfers.empty() || !graphics.empty() || failed) {
            std::fprintf(stderr, "native transfer ordering/cache failed at frame %u (%s)\n", frame,
                         negative);
            return 1;
        }
    }
    for (unsigned frame = 0; frame < 60; ++frame) {
        Pixels old = sample(mask), next, expected = old;
        for (unsigned i = 0; i < next.size(); ++i)
            next[i] = 50000 + frame * 101 + i * 19;
        for (unsigned y = 1; y < 3; ++y)
            for (unsigned x = 1; x < 3; ++x)
                expected[y * 4 + x] = next[y * 4 + x];
        graphics.push_back([old] {
            failed |= sample(mask) != old;
            observed_reads++;
        });
        graphics.push_back([next] { source.memory = next; });
        DkImageRect region{1, 1, 0, 2, 2, 1};
        nd_snapshot_commands(nullptr, &source.view, &mask.view, &region);
        if (sample(mask) != expected || failed)
            return 1;
    }
    if (observed_reads != 180 || copies != 180 || barrier_count != 360)
        return 1;
    std::puts(
        "180 delayed full/cropped blit/copy/upload snapshots: prior readers, source writes, completion and cache PASS");
    return 0;
}
