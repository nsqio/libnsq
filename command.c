#include <stdio.h>
#include <evbuffsock.h>

void nsq_subscribe(struct Buffer *buf, const char *topic, const char *channel)
{
    char b[128];
    size_t n;

    n = sprintf(b, "SUB %s %s\n", topic, channel);
    buffer_add(buf, b, n);
}

void nsq_ready(struct Buffer *buf, int count)
{
    char b[16];
    size_t n;

    n = sprintf(b, "RDY %d\n", count);
    buffer_add(buf, b, n);
}

void nsq_finish(struct Buffer *buf, const char *id)
{
    char b[48];
    size_t n;

    n = sprintf(b, "FIN %s\n", id);
    buffer_add(buf, b, n);
}

void nsq_requeue(struct Buffer *buf, const char *id, int timeout_ms)
{
    char b[128];
    size_t n;

    n = sprintf(b, "REQ %s %d\n", id, timeout_ms);
    buffer_add(buf, b, n);
}

void nsq_nop(struct Buffer *buf)
{
    buffer_add(buf, "NOP\n", 4);
}
