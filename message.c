#include "nsq.h"

#ifdef DEBUG
#define _DEBUG(...) fprintf(stdout, __VA_ARGS__)
#else
#define _DEBUG(...) do {;} while (0)
#endif

uint64_t ntoh64(const uint8_t *data) {
    return (uint64_t)(data[7]) | (uint64_t)(data[6])<<8 |
        (uint64_t)(data[5])<<16 | (uint64_t)(data[4])<<24 |
        (uint64_t)(data[3])<<32 | (uint64_t)(data[2])<<40 |
        (uint64_t)(data[1])<<48 | (uint64_t)(data[0])<<56;
}

struct NSQMessage *nsq_decode_message(const char *data, size_t data_length)
{
    struct NSQMessage *msg;
    size_t body_length;

    msg = malloc(sizeof(struct NSQMessage));
    msg->timestamp = (int64_t)ntoh64((uint8_t *)data);
    msg->attempts = ntohs(*(uint16_t *)(data+8));
    memcpy(&msg->id, data+10, 16);
    body_length = data_length - 26;
    msg->body = malloc(body_length);
    memcpy(msg->body, data+26, body_length);
    msg->body_length = body_length;

    return msg;
}

void free_nsq_message(struct NSQMessage *msg)
{
    if (msg) {
        free(msg->body);
        free(msg);
    }
}
