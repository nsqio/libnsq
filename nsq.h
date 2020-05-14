#ifndef __nsq_h
#define __nsq_h

#include <ev.h>
#include <evbuffsock.h>

#ifdef DEBUG
#define _DEBUG(...) fprintf(stdout, __VA_ARGS__)
#else
#define _DEBUG(...) do {;} while (0)
#endif

typedef enum {NSQ_FRAME_TYPE_RESPONSE, NSQ_FRAME_TYPE_ERROR, NSQ_FRAME_TYPE_MESSAGE} frame_type;
typedef enum {NSQ_PARAM_TYPE_INT, NSQ_PARAM_TYPE_CHAR} nsq_cmd_param_type;
typedef struct Buffer nsqBuf;
typedef struct BufferedSocket nsqBufdSock;
typedef struct NSQCmdParams {
    void *v;
    int t;
} nsqCmdParams;

typedef struct NSQMessage {
    int64_t timestamp;
    uint16_t attempts;
    char id[16+1];
    size_t body_length;
    char *body;
} nsqMsg;
typedef struct NSQLookupdEndpoint {
    char *address;
    int port;
    struct NSQLookupdEndpoint *next;
} nsqLE;
typedef struct NSQDConnection {
    char *address;
    int port;
    struct BufferedSocket *bs;
    struct Buffer *command_buf;
    uint32_t current_msg_size;
    uint32_t current_frame_type;
    char *current_data;
    struct ev_loop *loop;
    ev_timer *reconnect_timer;
    void (*connect_callback)(struct NSQDConnection *conn, void *arg);
    void (*close_callback)(struct NSQDConnection *conn, void *arg);
    void (*msg_callback)(struct NSQDConnection *conn, nsqMsg *msg, void *arg);
    void *arg;
    struct NSQDConnection *next;
} nsqdConn;
typedef struct NSQReaderCfg {
    ev_tstamp lookupd_interval;
    size_t command_buf_len;
    size_t command_buf_capacity;
    size_t read_buf_len;
    size_t read_buf_capacity;
    size_t write_buf_len;
    size_t write_buf_capacity;
} nsqRdrCfg;

typedef struct NSQReader {
    char *topic;
    char *channel;
    void *ctx; //context for call back
    int max_in_flight;
    nsqdConn *conns;
    struct NSQDConnInfo *infos;
    nsqLE *lookupd;
    struct ev_timer lookupd_poll_timer;
    struct ev_loop *loop;
    nsqRdrCfg *cfg;
    void *httpc;
    void (*connect_callback)(struct NSQReader *rdr, nsqdConn *conn);
    void (*close_callback)(struct NSQReader *rdr, nsqdConn *conn);
    void (*msg_callback)(struct NSQReader *rdr, nsqdConn *conn, nsqMsg *msg, void *ctx);
} nsqRdr;

nsqRdr *new_nsq_reader(struct ev_loop *loop, const char *topic, const char *channel, void *ctx,
    nsqRdrCfg *cfg,
    void (*connect_callback)(nsqRdr *rdr, nsqdConn *conn),
    void (*close_callback)(nsqRdr *rdr, nsqdConn *conn),
    void (*msg_callback)(nsqRdr *rdr, nsqdConn *conn, nsqMsg *msg, void *ctx));
void free_nsq_reader(nsqRdr *rdr);
int nsq_reader_connect_to_nsqd(nsqRdr *rdr, const char *address, int port);
int nsq_reader_connect_to_nsqlookupd(nsqRdr *rdr);
int nsq_reader_add_nsqlookupd_endpoint(nsqRdr *rdr, const char *address, int port);
void nsq_reader_set_loop(nsqRdr *rdr, struct ev_loop *loop);
void nsq_run(struct ev_loop *loop);

nsqdConn *new_nsqd_connection(struct ev_loop *loop, const char *address, int port,
    void (*connect_callback)(nsqdConn *conn, void *arg),
    void (*close_callback)(nsqdConn *conn, void *arg),
    void (*msg_callback)(nsqdConn *conn, nsqMsg *msg, void *arg),
    void *arg);
void free_nsqd_connection(nsqdConn *conn);
int nsqd_connection_connect(nsqdConn *conn);
void nsqd_connection_disconnect(nsqdConn *conn);

void nsqd_connection_init_timer(nsqdConn *conn,
        void (*reconnect_callback)(EV_P_ ev_timer *w, int revents));
void nsqd_connection_stop_timer(nsqdConn *conn);

void nsq_buffer_add(nsqBuf *buf, const char *name, const nsqCmdParams params[], size_t psize, const char *body, const size_t body_length);

void nsq_subscribe(nsqBuf *buf, const char *topic, const char *channel);
void nsq_ready(nsqBuf *buf, int count);
void nsq_finish(nsqBuf *buf, const char *id);
void nsq_requeue(nsqBuf *buf, const char *id, int timeout_ms);
void nsq_nop(nsqBuf *buf);
void nsq_publish(nsqBuf *buf, const char *topic, const char *body);
void nsq_multi_publish(nsqBuf *buf, const char *topic, const char **body, const size_t body_size);
void nsq_defer_publish(nsqBuf *buf, const char *topic, const char *body, int defer_time_sec);
void nsq_touch(nsqBuf *buf, const char *id);
void nsq_cleanly_close_connection(nsqBuf *buf);
void nsq_auth(nsqBuf *buf, const char *secret);
void nsq_identify(nsqBuf *buf, const char *json_body);

nsqMsg *nsq_decode_message(const char *data, size_t data_length);
void free_nsq_message(nsqMsg *msg);

nsqLE *new_nsqlookupd_endpoint(const char *address, int port);
void free_nsqlookupd_endpoint(nsqLE *nsqlookupd_endpoint);

#endif
