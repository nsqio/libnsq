#include "nsq.h"

#ifdef DEBUG
#define _DEBUG(...) fprintf(stdout, __VA_ARGS__)
#else
#define _DEBUG(...) do {;} while (0)
#endif

static void on_data(struct NSQReader *rdr, struct NSQDConnection *conn, 
    uint32_t frame_type, uint32_t msg_size, char *data)
{
    _DEBUG("%s: %d, %d, %.*s\n", __FUNCTION__, frame_type, msg_size, msg_size, data);
}

int main(int argc, char **argv)
{
    struct NSQReader *rdr;
    
    rdr = new_nsq_reader("test", "ch",
        NULL, NULL, on_data);
    nsq_reader_connect_to_nsqd(rdr, "127.0.0.1", 4150);
    nsq_run(ev_default_loop(0));
    
    return 0;
}
