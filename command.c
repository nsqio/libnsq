#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "nsq.h"

static const int defaultBufSize = 32;
static const int restBufSize    = 16;
static const int bufDelter      = 1;

void *nsq_buf_malloc(size_t *buf_size, size_t n, size_t l)
{
    int realLeftSize = (int)(*buf_size - n - l);

    if (realLeftSize >= restBufSize) {
        return NULL;
    }

    *buf_size += (restBufSize - realLeftSize) << bufDelter;
    
    void *buf = NULL;
    
    buf = malloc(*buf_size);
    assert(NULL != buf);
    
    return buf;
}

void nsq_buffer_add(nsqBuf *buf, const char *name, const nsqCmdParams params[], size_t psize, const char *body, const size_t body_length)
{
    size_t buf_size = defaultBufSize;
    char *b = malloc(buf_size * sizeof(char));
    char *nb = NULL;
    assert(NULL != b);
    size_t n = 0;
    size_t l = 0;

    l = strlen(name);
    memcpy(b, name, l);
    n += l;

    if (NULL != params) {
        for (size_t i = 0; i < psize; i++) {
            memcpy(b+n, " ", 1);
            n += 1;

            switch (params[i].t) {
                case NSQ_PARAM_TYPE_INT:
                    l = sprintf(b+n, "%d", *((int *)params[i].v));
                    break;
                case NSQ_PARAM_TYPE_CHAR:
                    l = strlen((char *)params[i].v);
                    nb = nsq_buf_malloc(&buf_size, n, l);
                    if (NULL != nb) {
                        memcpy(nb, b, n);
                        free(b);
                        b = nb;
                    }
                    memcpy(b+n, (char *)params[i].v, l);
                    break;
            }
            n += l;
        }
    }
    memcpy(b+n, "\n", 1);
    n += 1;

    if (NULL != body) {
        uint32_t vv = htonl((uint32_t)body_length);
        memcpy(b+n, &vv, 4);
        n += 4;

        nb = nsq_buf_malloc(&buf_size, n, body_length);
        if (NULL != nb) {
            memcpy(nb, b, n);
            free(b);
            b = nb;
        }
        memcpy(b+n, body, body_length);
        n += body_length;
    }

    buffer_add(buf, b, n);
}

void nsq_subscribe(nsqBuf *buf, const char *topic, const char *channel)
{
    const char *name = "SUB";
    const nsqCmdParams params[2] = {
        {(void *)topic, NSQ_PARAM_TYPE_CHAR},
        {(void *)channel, NSQ_PARAM_TYPE_CHAR},
    };
    nsq_buffer_add(buf, name, params, 2, NULL, 0);
}

void nsq_ready(nsqBuf *buf, int count)
{
    const char *name = "RDY";
    const nsqCmdParams params[1] = {
        {&count, NSQ_PARAM_TYPE_INT},
    };
    nsq_buffer_add(buf, name, params, 1, NULL, 0);
}

void nsq_finish(nsqBuf *buf, const char *id)
{
    const char *name = "FIN";
    const nsqCmdParams params[1] = {
        {(void *)id, NSQ_PARAM_TYPE_CHAR},
    };
    nsq_buffer_add(buf, name, params, 1, NULL, 0);
}

void nsq_requeue(nsqBuf *buf, const char *id, int timeout_ms)
{
    const char *name = "REQ";
    const nsqCmdParams params[2] = {
        {(void *)id, NSQ_PARAM_TYPE_CHAR},
        {&timeout_ms, NSQ_PARAM_TYPE_INT},
    };
    nsq_buffer_add(buf, name, params, 2, NULL, 0);
}

void nsq_nop(nsqBuf *buf)
{
    nsq_buffer_add(buf, "NOP", NULL, 0, NULL, 0);
}

void nsq_publish(nsqBuf *buf, const char *topic, const char *body)
{
    const char *name = "PUB";
    const nsqCmdParams params[1] = {
        {(void *)topic, NSQ_PARAM_TYPE_CHAR},
    };
    nsq_buffer_add(buf, name, params, 1, body, strlen(body));
}

void nsq_defer_publish(nsqBuf *buf, const char *topic, const char *body, int defer_time_sec)
{
    const char *name = "DPUB";
    const nsqCmdParams params[2] = {
        {(void *)topic, NSQ_PARAM_TYPE_CHAR},
        {&defer_time_sec, NSQ_PARAM_TYPE_INT},
    };
    nsq_buffer_add(buf, name, params, 2, body, strlen(body));
}

void nsq_multi_publish(nsqBuf *buf, const char *topic, const char **body, const size_t body_size)
{
    const char *name = "MPUB";
    const nsqCmdParams params[1] = {
        {(void *)topic, NSQ_PARAM_TYPE_CHAR},
    };

    size_t s = 4;
    for (size_t i = 0; i<body_size; i++) {
        s += strlen(body[i])+4;
    }
    char *b = malloc(s * sizeof(char));
    assert(NULL != b);

    size_t n = 0;
    uint32_t v = 0;
    v = htonl((uint32_t)body_size);
    memcpy(b+n, &v, 4);
    n += 4;

    size_t l = 0;
    for (size_t i = 0; i < body_size; i++) {
        l = strlen(body[i]);
        v = htonl((uint32_t)l);
        memcpy(b+n, &v, 4);
        n += 4;

        l = strlen(body[i]);
        memcpy(b+n, body[i], l);
        n += l;
    }

    nsq_buffer_add(buf, name, params, 1, b, s);
}

void nsq_touch(nsqBuf *buf, const char *id)
{
    const char *name = "TOUCH";
    const nsqCmdParams params[1] = {
        {(void *)id, NSQ_PARAM_TYPE_CHAR},
    };
    nsq_buffer_add(buf, name, params, 1, NULL, 0);
}

void nsq_cleanly_close_connection(nsqBuf *buf)
{
    const char *name = "CLS";
    nsq_buffer_add(buf, name, NULL, 0, NULL, 0);
}

void nsq_auth(nsqBuf *buf, const char *secret)
{
    const char *name = "AUTH";
    nsq_buffer_add(buf, name, NULL, 0, secret, strlen(secret));
}

//TODO: should handle object to json string
void nsq_identify(nsqBuf *buf, const char *json_body)
{
    const char *name = "IDENTIFY";
    nsq_buffer_add(buf, name, NULL, 1, json_body, strlen(json_body));
}
