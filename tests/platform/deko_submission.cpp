/* Run the production submission helper with an asynchronous SDK command sink. */
#include "../../src/platform/switch/deko_submission.h"
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

namespace {
struct Command {
    unsigned id;
    bool transfer;
};
std::vector<Command> recorded, queued;
std::deque<std::vector<Command>> lists;
unsigned flushes, total_transfers, max_transfers, pending_transfers;
bool failed;
const char *negative;
} // namespace

extern "C" DkCmdList dkCmdBufFinishList(DkCmdBuf)
{
    lists.push_back(recorded);
    if (std::strcmp(negative, "--duplicate-negative"))
        recorded.clear();
    return reinterpret_cast<DkCmdList>(&lists.back());
}
extern "C" void dkQueueSubmitCommands(DkQueue, DkCmdList list)
{
    const auto &commands = *reinterpret_cast<const std::vector<Command> *>(list);
    for (auto c : commands) {
        pending_transfers += c.transfer;
        queued.push_back(c);
    }
}
extern "C" void dkQueueFlush(DkQueue)
{
    max_transfers = pending_transfers > max_transfers ? pending_transfers : max_transfers;
    pending_transfers = 0;
    flushes++;
    /* Enqueue only: do not consume commands or reclaim backing storage here. */
}
extern "C" void dkQueueWaitIdle(DkQueue)
{
    failed = true; // A chunk boundary must never wait for GPU completion.
}
int main(int argc, char **argv)
{
    negative = argc > 1 ? argv[1] : "";
    nd_submission pending{};
    std::vector<Command> expected;
    size_t bytes = 0;
    unsigned id = 0;
    for (unsigned frame = 0; frame < 4; frame++) {
        unsigned count = frame == 0 ? 592 : frame == 1 ? 1 : frame == 2 ? 64 : 17;
        for (unsigned upload = 0; upload < count; upload++) {
            /* Interleave draws and copy/upload barriers in the same command
               buffer. Partial submissions must not duplicate earlier ranges. */
            Command draw{++id, false}, transfer{++id, true};
            recorded.push_back(draw);
            recorded.push_back(transfer);
            expected.push_back(draw);
            expected.push_back(transfer);
            size_t size = frame == 0   ? 60157548 / 592 + (upload < 60157548 % 592)
                          : frame == 1 ? 5 * 1024 * 1024
                                       : 1024;
            bytes += size;
            if (std::strcmp(negative, "--negative"))
                nd_submit_transfer(pending, nullptr, nullptr,
                                   std::strcmp(negative, "--bytes-negative") ? size : 0);
            total_transfers++;
        }
        if (frame == 1 && pending.transfers)
            failed = true; // A single upload exceeding the byte budget submits immediately.
        nd_submit_pending(pending, nullptr, nullptr);
        /* Model the final frame fence after every chunk. Submission itself
           does not complete that fence or reset the recording allocation. */
        if (queued.size() != expected.size())
            failed = true;
        for (size_t i = 0; i < queued.size() && i < expected.size(); i++)
            failed |= queued[i].id != expected[i].id || queued[i].transfer != expected[i].transfer;
    }
    failed |= max_transfers > 32 || total_transfers != 674 || !flushes;
    if (failed) {
        std::fprintf(stderr, "submission failed: max_transfers=%u queued=%zu expected=%zu (%s)\n",
                     max_transfers, queued.size(), expected.size(), negative);
        return 1;
    }
    std::printf(
        "674 transfers / %zu bytes: FIFO exactly once, max chunk %u, %u flushes, no idle wait\n",
        bytes, max_transfers, flushes);
    return 0;
}
