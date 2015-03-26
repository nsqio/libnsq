#include "nsq.h"
#include "utlist.h"
#include "http.h"

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

    // subscribe
    buffer_reset(conn->command_buf);
    nsq_subscribe(conn->command_buf, rdr->topic, rdr->channel);
    buffered_socket_write_buffer(conn->bs, conn->command_buf);

    // send initial RDY
    buffer_reset(conn->command_buf);
    nsq_ready(conn->command_buf, rdr->max_in_flight);
    buffered_socket_write_buffer(conn->bs, conn->command_buf);
}

static void nsq_reader_msg_cb(struct NSQDConnection *conn, struct NSQMessage *msg, void *arg)
{
    struct NSQReader *rdr = (struct NSQReader *)arg;

    _DEBUG("%s: %p %p\n", __FUNCTION__, msg, rdr);

    if (rdr->msg_callback) {
        msg->id[sizeof(msg->id)-1] = '\0';
        rdr->msg_callback(rdr, conn, msg, rdr->ctx);
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

    free_nsqd_connection(conn);
}

void nsq_lookupd_request_cb(struct HttpRequest *req, struct HttpResponse *resp, void *arg);

static void nsq_reader_lookupd_poll_cb(EV_P_ struct ev_timer *w, int revents)
{
    struct NSQReader *rdr = (struct NSQReader *)w->data;
    struct NSQLookupdEndpoint *nsqlookupd_endpoint;
    struct HttpRequest *req;
    int i, idx, count = 0;
    char buf[256];

    LL_FOREACH(rdr->lookupd, nsqlookupd_endpoint) {
        count++;
    }
    idx = rand() % count;

    _DEBUG("%s: rdr %p (chose %d)\n", __FUNCTION__, rdr, idx);

    i = 0;
    LL_FOREACH(rdr->lookupd, nsqlookupd_endpoint) {
        if (i++ == idx) {
            sprintf(buf, "http://%s:%d/lookup?topic=%s", nsqlookupd_endpoint->address,
                nsqlookupd_endpoint->port, rdr->topic);
            req = new_http_request(buf, nsq_lookupd_request_cb, rdr);
            http_client_get((struct HttpClient *)rdr->httpc, req);
            break;
        }
    }

    ev_timer_again(rdr->loop, &rdr->lookupd_poll_timer);
}

struct NSQReader *new_nsq_reader(struct ev_loop *loop, const char *topic, const char *channel, void *ctx,
    void (*connect_callback)(struct NSQReader *rdr, struct NSQDConnection *conn),
    void (*close_callback)(struct NSQReader *rdr, struct NSQDConnection *conn),
    void (*msg_callback)(struct NSQReader *rdr, struct NSQDConnection *conn, struct NSQMessage *msg, void *ctx))
{
    struct NSQReader *rdr;

    rdr = (struct NSQReader *)malloc(sizeof(struct NSQReader));
    rdr->topic = strdup(topic);
    rdr->channel = strdup(channel);
    rdr->max_in_flight = 1;
    rdr->connect_callback = connect_callback;
    rdr->close_callback = close_callback;
    rdr->msg_callback = msg_callback;
    rdr->ctx = ctx;
    rdr->conns = NULL;
    rdr->lookupd = NULL;
    rdr->loop = loop;

    rdr->httpc = new_http_client(rdr->loop);

    // TODO: configurable interval
    ev_timer_init(&rdr->lookupd_poll_timer, nsq_reader_lookupd_poll_cb, 0., 5.);
    rdr->lookupd_poll_timer.data = rdr;
    ev_timer_again(rdr->loop, &rdr->lookupd_poll_timer);

    return rdr;
}

void free_nsq_reader(struct NSQReader *rdr)
{
    struct NSQDConnection *conn;
    struct NSQLookupdEndpoint *nsqlookupd_endpoint;

    if (rdr) {
        // TODO: this should probably trigger disconnections and then keep
        // trying to clean up until everything upstream is finished
        LL_FOREACH(rdr->conns, conn) {
            nsqd_connection_disconnect(conn);
        }
        LL_FOREACH(rdr->lookupd, nsqlookupd_endpoint) {
            free_nsqlookupd_endpoint(nsqlookupd_endpoint);
        }
        free(rdr->topic);
        free(rdr->channel);
        free(rdr);
    }
}

int nsq_reader_add_nsqlookupd_endpoint(struct NSQReader *rdr, const char *address, int port)
{
    struct NSQLookupdEndpoint *nsqlookupd_endpoint;

    nsqlookupd_endpoint = new_nsqlookupd_endpoint(address, port);
    LL_APPEND(rdr->lookupd, nsqlookupd_endpoint);

    return 1;
}

int nsq_reader_connect_to_nsqd(struct NSQReader *rdr, const char *address, int port)
{
    struct NSQDConnection *conn;
    int rc;

    conn = new_nsqd_connection(rdr->loop, address, port,
        nsq_reader_connect_cb, nsq_reader_close_cb, nsq_reader_msg_cb, rdr);
    rc = nsqd_connection_connect(conn);
    if (rc > 0) {
        LL_APPEND(rdr->conns, conn);
    }
    return rc;
}

void nsq_run(struct ev_loop *loop)
{
    srand(time(NULL));
    ev_loop(loop, 0);
}
