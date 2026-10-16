/* Switch work budget, not a claimed driver limit or an original-game value. */
#ifndef MOON_DEKO_SUBMISSION_H
#define MOON_DEKO_SUBMISSION_H
#include <deko3d.h>
#include <stddef.h>

struct nd_submission {
    unsigned transfers{};
    size_t bytes{};
};

static inline void nd_submit_pending(nd_submission &pending, DkQueue queue, DkCmdBuf commands)
{
    /* finishList advances the recorded range without clearing command memory.
       FIFO queue order and the eventual frame fence cover all earlier chunks.
       dk_queue.cpp/dk_cmdbuf.cpp (SDK 0.5): Flush submits, WaitIdle waits. */
    dkQueueSubmitCommands(queue, dkCmdBufFinishList(commands));
    dkQueueFlush(queue);
    pending = {};
}

static inline bool nd_submit_transfer(nd_submission &pending, DkQueue queue, DkCmdBuf commands,
                                      size_t bytes)
{
    /* P: cap loading bursts at 32 completed transfers or 4 MiB of uploads.
       log14 accumulated 592 / 60157548 bytes before kickoff was rejected.
       This is a bounded submission policy; the driver did not report a limit. */
    pending.transfers++;
    pending.bytes += bytes;
    if (pending.transfers < 32 && pending.bytes < 4 * 1024 * 1024)
        return false;
    nd_submit_pending(pending, queue, commands);
    return true;
}
#endif
