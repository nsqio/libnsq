#ifndef __nsq_h
#define __nsq_h

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <utlist.h>
#include <ev.h>
#include <evbuffsock.h>

struct NSQDConnection;
struct NSQReader;

typedef void (*NSQReaderCallback)(struct NSQReader *rdr, struct NSQDConnection *conn);

struct NSQReader {
    char *topic;
    char *channel;
    int max_in_flight;
    struct NSQDConnection *conns;
    NSQReaderCallback connect_callback;
    NSQReaderCallback close_callback;
    NSQReaderCallback data_callback;;
};

struct NSQReader *new_nsq_reader(const char *topic, const char *channel,
    NSQReaderCallback connect_callback,
    NSQReaderCallback close_callback,
    NSQReaderCallback data_callback);
void free_nsq_reader(struct NSQReader *rdr);
int nsq_reader_connect_to_nsqd(struct NSQReader *rdr, const char *address, int port);
void nsq_run(struct ev_loop *loop);

typedef void (*NSQDConnectionCallback)(struct NSQDConnection *conn, void *arg);

struct NSQDConnection {
    struct BufferedSocket *bs;
    struct Buffer *command_buf;
    size_t current_msg_size;
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

#endif
