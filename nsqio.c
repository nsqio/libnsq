#include <assert.h>

#include "nsq.h"
#include "utlist.h"

#define DEFAULT_LOOKUPD_INTERVAL     5.
#define DEFAULT_COMMAND_BUF_LEN      4096
#define DEFAULT_COMMAND_BUF_CAPACITY 4096
#define DEFAULT_READ_BUF_LEN         16 * 1024
#define DEFAULT_READ_BUF_CAPACITY    16 * 1024
#define DEFAULT_WRITE_BUF_LEN        16 * 1024
#define DEFAULT_WRITE_BUF_CAPACITY   16 * 1024

nsqio *new_nsqio(struct ev_loop *loop, const char *topic, const char *channel, void *ctx,
    void (*connect_callback)(nsqio *nio, nsqdConn *conn),
    void (*close_callback)(nsqio *nio, nsqdConn *conn),
    void (*msg_callback)(nsqio *nio, nsqdConn *conn, nsqMsg *msg, void *ctx),
    nsqLookupdMode mode)
{
    nsqio *nio;

    nio = (nsqio *)malloc(sizeof(nsqio));
    
    nsqCfg *cfg = (nsqCfg *)malloc(sizeof(nsqCfg));
    assert(NULL != cfg);

    if (cfg == NULL) {
        cfg->lookupd_interval     = DEFAULT_LOOKUPD_INTERVAL;
        cfg->command_buf_len      = DEFAULT_COMMAND_BUF_LEN;
        cfg->command_buf_capacity = DEFAULT_COMMAND_BUF_CAPACITY;
        cfg->read_buf_len         = DEFAULT_READ_BUF_LEN;
        cfg->read_buf_capacity    = DEFAULT_READ_BUF_CAPACITY;
        cfg->write_buf_len        = DEFAULT_WRITE_BUF_LEN;
        cfg->write_buf_capacity   = DEFAULT_WRITE_BUF_CAPACITY;
    } else {
        cfg->lookupd_interval     = cfg->lookupd_interval     <= 0 ? DEFAULT_LOOKUPD_INTERVAL     : cfg->lookupd_interval;
        cfg->command_buf_len      = cfg->command_buf_len      <= 0 ? DEFAULT_COMMAND_BUF_LEN      : cfg->command_buf_len;
        cfg->command_buf_capacity = cfg->command_buf_capacity <= 0 ? DEFAULT_COMMAND_BUF_CAPACITY : cfg->command_buf_capacity;
        cfg->read_buf_len         = cfg->read_buf_len         <= 0 ? DEFAULT_READ_BUF_LEN         : cfg->read_buf_len;
        cfg->read_buf_capacity    = cfg->read_buf_capacity    <= 0 ? DEFAULT_READ_BUF_CAPACITY    : cfg->read_buf_capacity;
        cfg->write_buf_len        = cfg->write_buf_len        <= 0 ? DEFAULT_WRITE_BUF_LEN        : cfg->write_buf_len;
        cfg->write_buf_capacity   = cfg->write_buf_capacity   <= 0 ? DEFAULT_WRITE_BUF_CAPACITY   : cfg->write_buf_capacity;
    }
    nio->cfg = cfg;

    assert(NULL != topic);
    nio->topic = strdup(topic);
    if (NULL != channel) {
        nio->channel = strdup(channel);
    }
    nio->max_in_flight = 1;
    nio->connect_callback = connect_callback;
    nio->close_callback = close_callback;
    nio->msg_callback = msg_callback;
    nio->ctx = ctx;
    nio->conns = NULL;
    nio->lookupd = NULL;
    nio->loop = loop;
    nio->lookupd_poll_timer = malloc(sizeof(struct ev_timer));

    nio->httpc = new_http_client(loop);
    nio->mode = mode;

    return nio;
}

void free_nsqio(nsqio *nio)
{
    nsqdConn *conn;
    nsqLookupdEndpoint *nsqlookupd_endpoint;

    if (nio) {
        // TODO: this should probably trigger disconnections and then keep
        // trying to clean up until everything upstream is finished
        LL_FOREACH(nio->conns, conn) {
            nsqd_connection_disconnect(conn);
        }
        LL_FOREACH(nio->lookupd, nsqlookupd_endpoint) {
            free_nsqlookupd_endpoint(nsqlookupd_endpoint);
        }
        free(nio->topic);
        free(nio->channel);
        free(nio->cfg);
        free(nio->lookupd_poll_timer);
        free(nio->httpc->timer_event);
        free(nio->httpc);
        free(nio);
    }
}
