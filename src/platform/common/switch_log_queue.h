/* Bounded diagnostic transport. Callers serialize push/pop; a full queue drops
   diagnostics instead of making the render thread wait for storage or sockets. */
#ifndef MOON_SWITCH_LOG_QUEUE_H
#define MOON_SWITCH_LOG_QUEUE_H
#include <string.h>
enum { SWP_LOG_CAPACITY = 32, SWP_LOG_LINE = 4096 };
typedef struct {
    char lines[SWP_LOG_CAPACITY][SWP_LOG_LINE];
    unsigned head, count;
} swp_log_queue;
static int swp_log_push(swp_log_queue *queue, const char *line)
{
    size_t size = strlen(line);
    if (queue->count == SWP_LOG_CAPACITY || size >= SWP_LOG_LINE)
        return 0;
    unsigned slot = (queue->head + queue->count) % SWP_LOG_CAPACITY;
    memcpy(queue->lines[slot], line, size + 1);
    queue->count++;
    return 1;
}
static int swp_log_pop(swp_log_queue *queue, char out[SWP_LOG_LINE])
{
    if (!queue->count)
        return 0;
    memcpy(out, queue->lines[queue->head], SWP_LOG_LINE);
    queue->head = (queue->head + 1) % SWP_LOG_CAPACITY;
    queue->count--;
    return 1;
}
#endif
