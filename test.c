#include "nsq.h"

int main(int argc, char **argv)
{
    struct NSQReader *rdr;
    
    rdr = new_nsq_reader("test", "ch",
        NULL, NULL, NULL);
    nsq_reader_connect_to_nsqd(rdr, "127.0.0.1", 4150);
    nsq_run(ev_default_loop(0));
    
    return 0;
}
