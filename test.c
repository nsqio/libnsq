#include "nsq.h"

#ifdef DEBUG
#define _DEBUG(...) fprintf(stdout, __VA_ARGS__)
#else
#define _DEBUG(...) do {;} while (0)
#endif

static void message_handler(struct NSQReader *rdr, struct NSQMessage *msg)
{
    _DEBUG("%s: %lld, %d, %s, %lu, %.*s\n", __FUNCTION__, msg->timestamp, msg->attempts, msg->id, 
        msg->body_length, (int)msg->body_length, msg->body);
}

int main(int argc, char **argv)
{
    struct NSQReader *rdr;
    
    rdr = new_nsq_reader("test", "ch",
        NULL, NULL, message_handler);
    nsq_reader_connect_to_nsqd(rdr, "127.0.0.1", 4150);
    nsq_run(ev_default_loop(0));
    
    return 0;
}
