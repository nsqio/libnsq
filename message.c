#include "nsq.h"

#ifdef DEBUG
#define _DEBUG(...) fprintf(stdout, __VA_ARGS__)
#else
#define _DEBUG(...) do {;} while (0)
#endif

uint64_t ntoh64(const uint64_t *input)
{
    uint64_t rval;
    uint8_t *data = (uint8_t *)&rval;
    
    data[0] = *input >> 56;
    data[1] = *input >> 48;
    data[2] = *input >> 40;
    data[3] = *input >> 32;
    data[4] = *input >> 24;
    data[5] = *input >> 16;
    data[6] = *input >> 8;
    data[7] = *input >> 0;
    
    return rval;
}

struct NSQMessage *nsq_decode_message(const char *data, size_t data_length)
{
    struct NSQMessage *msg;
    size_t body_length;
    
    msg = malloc(sizeof(struct NSQMessage));
    msg->timestamp = (int64_t)ntoh64((uint64_t *)data);
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
