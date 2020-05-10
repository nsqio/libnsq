#include <string.h>
#include <time.h>

#include "http.h"
#include "nsq.h"
#include "utlist.h"

static void nsq_reader_connect_cb(nsqdConn *conn, void *arg)
{
    nsqio *rdr = (nsqio *)arg;

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

static void nsq_reader_msg_cb(nsqdConn *conn, nsqMsg *msg, void *arg)
{
    nsqio *rdr = (nsqio *)arg;

    _DEBUG("%s: %p %p\n", __FUNCTION__, msg, rdr);

    if (rdr->msg_callback) {
        msg->id[sizeof(msg->id)-1] = '\0';
        rdr->msg_callback(rdr, conn, msg, rdr->ctx);
    }
}

static void nsq_reader_close_cb(nsqdConn *conn, void *arg)
{
    nsqio *rdr = (nsqio *)arg;

    _DEBUG("%s: %p\n", __FUNCTION__, rdr);

    if (rdr->close_callback) {
        rdr->close_callback(rdr, conn);
    }

    LL_DELETE(rdr->conns, conn);

    // There is no lookupd, try to reconnect to nsqd directly
    if (rdr->lookupd == NULL) {
        ev_timer_again(conn->loop, conn->reconnect_timer);
    } else {
        free_nsqd_connection(conn);
    }
}

void nsq_lookupd_request_cb(httpRequest *req, httpResponse *resp, void *arg);

static void nsq_reader_reconnect_cb(EV_P_ struct ev_timer *w, int revents)
{
    nsqdConn *conn = (nsqdConn *)w->data;
    nsqio *rdr = (nsqio *)conn->arg;

    if (rdr->lookupd == NULL) {
        _DEBUG("%s: There is no lookupd, try to reconnect to nsqd directly\n", __FUNCTION__);
        nsq_reader_connect_to_nsqd(rdr, conn->address, conn->port);
    }

    free_nsqd_connection(conn);
}

static void nsq_reader_lookupd_poll_cb(EV_P_ struct ev_timer *w, int revents)
{
    nsqio *rdr = (nsqio *)w->data;
    nsqLookupdEndpoint *nsqlookupd_endpoint;
    httpRequest *req;
    int i, idx, count = 0;
    char buf[256];

    LL_FOREACH(rdr->lookupd, nsqlookupd_endpoint) {
        count++;
    }
    if(count == 0) {
        goto end;
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

end:
    ev_timer_again(rdr->loop, rdr->lookupd_poll_timer);
}

void free_nsq_reader(nsqio *rdr)
{
    nsqdConn *conn;
    nsqLookupdEndpoint *nsqlookupd_endpoint;

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
        free(rdr->cfg);
        free(rdr->lookupd_poll_timer);
        free(rdr);
    }
}

int nsq_reader_add_nsqlookupd_endpoint(nsqio *rdr, const char *address, int port)
{
    nsqLookupdEndpoint *nsqlookupd_endpoint;
    nsqdConn *conn;

    if (rdr->lookupd == NULL) {
        // Stop reconnect timers, use lookupd timer instead
        LL_FOREACH(rdr->conns, conn) {
            nsqd_connection_stop_timer(conn);
        }

        ev_timer_init(rdr->lookupd_poll_timer, nsq_reader_lookupd_poll_cb, 0., rdr->cfg->lookupd_interval);
        rdr->lookupd_poll_timer->data = rdr;
        ev_timer_again(rdr->loop, rdr->lookupd_poll_timer);
    }

    nsqlookupd_endpoint = new_nsqlookupd_endpoint(address, port);
    LL_APPEND(rdr->lookupd, nsqlookupd_endpoint);

    return 1;
}

int nsq_reader_connect_to_nsqd(nsqio *rdr, const char *address, int port)
{
    nsqdConn *conn;
    int rc;

    conn = new_nsqd_connection(rdr->loop, address, port,
        nsq_reader_connect_cb, nsq_reader_close_cb, nsq_reader_msg_cb, rdr);
    rc = nsqd_connection_connect(conn);
    if (rc > 0) {
        LL_APPEND(rdr->conns, conn);
    }

    if (rdr->lookupd == NULL) {
        nsqd_connection_init_timer(conn, nsq_reader_reconnect_cb);
    }

    return rc;
}

void nsq_reader_run(struct ev_loop *loop)
{
    srand((unsigned)time(NULL));
    ev_loop(loop, 0);
}
