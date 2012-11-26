#include "nsq.h"

#ifdef DEBUG
#define _DEBUG(...) fprintf(stdout, __VA_ARGS__)
#else
#define _DEBUG(...) do {;} while (0)
#endif

static void nsq_reader_connect_cb(struct NSQDConnection *conn, void *arg)
{
    struct NSQReader *rdr = (struct NSQReader *)arg;
    
    _DEBUG("%s: %p\n", __FUNCTION__, rdr);
    
    if (rdr->connect_callback) {
        rdr->connect_callback(rdr, conn);
    }
    
    // send magic
    buffered_socket_write(conn->bs, "  V2", 4);
    
    // subscribe
    buffer_reset(conn->command_buf);
    nsq_subscribe(conn->command_buf, rdr->topic, rdr->channel);
    buffered_socket_write_buffer(conn->bs, conn->command_buf);
    
    // send initial RDY
    buffer_reset(conn->command_buf);
    nsq_ready(conn->command_buf, rdr->max_in_flight);
    buffered_socket_write_buffer(conn->bs, conn->command_buf);
}

static void nsq_reader_data_cb(struct NSQDConnection *conn, void *arg)
{
    struct NSQReader *rdr = (struct NSQReader *)arg;
    
    _DEBUG("%s: %p\n", __FUNCTION__, rdr);
    
    switch (conn->current_frame_type) {
        case NSQ_FRAME_TYPE_RESPONSE:
            if (strncmp(conn->current_data, "_heartbeat_", 11) == 0) {
                buffer_reset(conn->command_buf);
                nsq_nop(conn->command_buf);
                buffered_socket_write_buffer(conn->bs, conn->command_buf);
                return;
            }
            break;
    }
    
    if (rdr->data_callback) {
        rdr->data_callback(rdr, conn, 
            conn->current_frame_type, conn->current_msg_size, conn->current_data);
    }
}

static void nsq_reader_close_cb(struct NSQDConnection *conn, void *arg)
{
    struct NSQReader *rdr = (struct NSQReader *)arg;
    
    _DEBUG("%s: %p\n", __FUNCTION__, rdr);
    
    if (rdr->close_callback) {
        rdr->close_callback(rdr, conn);
    }
    
    LL_DELETE(rdr->conns, conn);
}

struct NSQReader *new_nsq_reader(const char *topic, const char *channel,
    void (*connect_callback)(struct NSQReader *rdr, struct NSQDConnection *conn),
    void (*close_callback)(struct NSQReader *rdr, struct NSQDConnection *conn),
    void (*data_callback)(struct NSQReader *rdr, struct NSQDConnection *conn,
        uint32_t frame_type, uint32_t msg_size, char *data))
{
    struct NSQReader *rdr;
    
    rdr = malloc(sizeof(struct NSQReader));
    rdr->topic = strdup(topic);
    rdr->channel = strdup(channel);
    rdr->max_in_flight = 1;
    rdr->connect_callback = connect_callback;
    rdr->close_callback = close_callback;
    rdr->data_callback = data_callback;
    rdr->conns = NULL;
    
    return rdr;
}

void free_nsq_reader(struct NSQReader *rdr)
{
    if (rdr) {
        free(rdr->topic);
        free(rdr->channel);
        free(rdr);
    }
}

int nsq_reader_connect_to_nsqd(struct NSQReader *rdr, const char *address, int port)
{
    struct NSQDConnection *conn;
    
    conn = new_nsqd_connection(address, port, 
        nsq_reader_connect_cb, nsq_reader_close_cb, nsq_reader_data_cb, rdr);
    LL_APPEND(rdr->conns, conn);
    
    return nsqd_connection_connect(conn);
}

void nsq_run(struct ev_loop *loop)
{
    ev_loop(loop, 0);
}
