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
    nsq_subscribe(conn->command_buf, "test", "ch");
    buffered_socket_write_buffer(conn->bs, conn->command_buf);
}

struct NSQReader *new_nsq_reader(const char *topic, const char *channel,
    NSQReaderCallback connect_callback,
    NSQReaderCallback close_callback,
    NSQReaderCallback data_callback)
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
        nsq_reader_connect_cb, NULL, NULL, rdr);
    LL_APPEND(rdr->conns, conn);
    
    return nsqd_connection_connect(conn);
}

void nsq_run(struct ev_loop *loop)
{
    ev_loop(loop, 0);
}
