#ifndef __nsq_h
#define __nsq_h

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <utlist.h>
#include <ev.h>
#include <evbuffsock.h>

enum {NSQ_FRAME_TYPE_RESPONSE, NSQ_FRAME_TYPE_ERROR, NSQ_FRAME_TYPE_MESSAGE} frame_type;
struct NSQDConnection;
struct NSQMessage;

struct NSQReader {
    char *topic;
    char *channel;
    int max_in_flight;
    struct NSQDConnection *conns;
    void (*connect_callback)(struct NSQReader *rdr, struct NSQDConnection *conn);
    void (*close_callback)(struct NSQReader *rdr, struct NSQDConnection *conn);
    void (*msg_callback)(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg);
};

struct NSQReader *new_nsq_reader(const char *topic, const char *channel,
    void (*connect_callback)(struct NSQReader *rdr, struct NSQDConnection *conn),
    void (*close_callback)(struct NSQReader *rdr, struct NSQDConnection *conn),
    void (*msg_callback)(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg));
void free_nsq_reader(struct NSQReader *rdr);
int nsq_reader_connect_to_nsqd(struct NSQReader *rdr, const char *address, int port);
void nsq_run(struct ev_loop *loop);

typedef void (*NSQDConnectionCallback)(struct NSQDConnection *conn, void *arg);

struct NSQDConnection {
    struct BufferedSocket *bs;
    struct Buffer *command_buf;
    uint32_t current_msg_size;
    uint32_t current_frame_type;
    char *current_data;
    NSQDConnectionCallback connect_callback;
    NSQDConnectionCallback close_callback;
    NSQDConnectionCallback data_callback;
    void *arg;
    struct NSQDConnection *next;
};

struct NSQDConnection *new_nsqd_connection(const char *address, int port, 
    NSQDConnectionCallback connect_callback,
    NSQDConnectionCallback close_callback,
    NSQDConnectionCallback data_callback,
    void *arg);
void free_nsqd_connection(struct NSQDConnection *conn);
int nsqd_connection_connect(struct NSQDConnection *conn);

void nsq_subscribe(struct Buffer *buf, const char *topic, const char *channel);
void nsq_ready(struct Buffer *buf, int count);
void nsq_finish(struct Buffer *buf, const char *id);
void nsq_requeue(struct Buffer *buf, const char *id, int timeout_ms);
void nsq_nop(struct Buffer *buf);

struct NSQMessage {
    int64_t timestamp;
    uint16_t attempts;
    char id[16];
    size_t body_length;
    char *body;
};

struct NSQMessage *nsq_decode_message(const char *data, size_t data_length);
void free_nsq_message(struct NSQMessage *msg);

#endif
