#include <stdio.h>
#include <evbuffsock.h>

const static char * NEW_LINE = "\n";
const static int MAX_BUF_SIZE = 128;

void nsq_subscribe(struct Buffer *buf, const char *topic, const char *channel)
{
    char b[MAX_BUF_SIZE];
    size_t n;

    n = sprintf(b, "SUB %s %s%s", topic, channel, NEW_LINE);
    buffer_add(buf, b, n);
}

void nsq_ready(struct Buffer *buf, int count)
{
    char b[MAX_BUF_SIZE];
    size_t n;

    n = sprintf(b, "RDY %d%s", count, NEW_LINE);
    buffer_add(buf, b, n);
}

void nsq_finish(struct Buffer *buf, const char *id)
{
    char b[MAX_BUF_SIZE];
    size_t n;

    n = sprintf(b, "FIN %s%s", id, NEW_LINE);
    buffer_add(buf, b, n);
}

void nsq_requeue(struct Buffer *buf, const char *id, int timeout_ms)
{
    char b[MAX_BUF_SIZE];
    size_t n;

    n = sprintf(b, "REQ %s %d%s", id, timeout_ms, NEW_LINE);
    buffer_add(buf, b, n);
}

void nsq_nop(struct Buffer *buf)
{
    char b[MAX_BUF_SIZE];
    size_t n;
    n = sprintf(b, "NOP%s", NEW_LINE);
    buffer_add(buf, b, n);
}
