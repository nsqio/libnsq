#include "nsq.h"

#ifdef DEBUG
#define _DEBUG(...) fprintf(stdout, __VA_ARGS__)
#else
#define _DEBUG(...) do {;} while (0)
#endif

static void message_handler(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg)
{
    _DEBUG("%s: %lld, %d, %s, %lu, %.*s\n", __FUNCTION__, msg->timestamp, msg->attempts, msg->id, 
        msg->body_length, (int)msg->body_length, msg->body);
    
    buffer_reset(conn->command_buf);
    nsq_finish(conn->command_buf, msg->id);
    buffered_socket_write_buffer(conn->bs, conn->command_buf);
    
    buffer_reset(conn->command_buf);
    nsq_ready(conn->command_buf, rdr->max_in_flight);
    buffered_socket_write_buffer(conn->bs, conn->command_buf);
}

int main(int argc, char **argv)
{
    struct NSQReader *rdr;
    struct ev_loop *loop;
    
    loop = ev_default_loop(0);
    rdr = new_nsq_reader(loop, "test", "ch",
        NULL, NULL, message_handler);
    nsq_reader_connect_to_nsqd(rdr, "127.0.0.1", 4150);
    nsq_reader_add_nsqlookupd_endpoint(rdr, "127.0.0.1", 4161);
    nsq_run(loop);
    
    return 0;
}
