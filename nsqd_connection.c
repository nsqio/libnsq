#include <string.h>

#include "nsq.h"

static void nsqd_connection_read_size(nsqBufdSock *buffsock, void *arg);
static void nsqd_connection_read_data(nsqBufdSock *buffsock, void *arg);

static void nsqd_connection_connect_cb(nsqBufdSock *buffsock, void *arg)
{
    nsqdConn *conn = (nsqdConn *)arg;

    _DEBUG("%s: %p\n", __FUNCTION__, arg);

    // send magic
    char buff[10] = "  V2";
    buffered_socket_write(conn->bs, (void *)buff, 4);

    if (conn->connect_callback) {
        conn->connect_callback(conn, conn->arg);
    }

    buffered_socket_read_bytes(buffsock, 4, nsqd_connection_read_size, conn);
}

static void nsqd_connection_read_size(nsqBufdSock *buffsock, void *arg)
{
    nsqdConn *conn = (nsqdConn *)arg;
    uint32_t *msg_size_be;

    _DEBUG("%s: %p\n", __FUNCTION__, arg);

    msg_size_be = (uint32_t *)buffsock->read_buf->data;
    buffer_drain(buffsock->read_buf, 4);

    // convert message length header from big-endian
    conn->current_msg_size = ntohl(*msg_size_be);

    _DEBUG("%s: msg_size = %d bytes %p\n", __FUNCTION__, conn->current_msg_size, buffsock->read_buf->data);

    buffered_socket_read_bytes(buffsock, conn->current_msg_size, nsqd_connection_read_data, conn);
}

static void nsqd_connection_read_data(nsqBufdSock *buffsock, void *arg)
{
    nsqdConn *conn = (nsqdConn *)arg;
    nsqMsg *msg;

    conn->current_frame_type = ntohl(*((uint32_t *)buffsock->read_buf->data));
    buffer_drain(buffsock->read_buf, 4);
    conn->current_msg_size -= 4;

    _DEBUG("%s: frame type %d, data: %.*s\n", __FUNCTION__, conn->current_frame_type,
        conn->current_msg_size, buffsock->read_buf->data);

    conn->current_data = buffsock->read_buf->data;
    switch (conn->current_frame_type) {
        case NSQ_FRAME_TYPE_RESPONSE:
            if (strncmp(conn->current_data, "_heartbeat_", 11) == 0) {
                buffer_reset(conn->command_buf);
                nsq_nop(conn->command_buf);
                buffered_socket_write_buffer(conn->bs, conn->command_buf);
            }
            break;
        case NSQ_FRAME_TYPE_MESSAGE:
            msg = nsq_decode_message(conn->current_data, conn->current_msg_size);
            if (conn->msg_callback) {
                conn->msg_callback(conn, msg, conn->arg);
            }
            break;
    }

    buffer_drain(buffsock->read_buf, conn->current_msg_size);

    buffered_socket_read_bytes(buffsock, 4, nsqd_connection_read_size, conn);
}

static void nsqd_connection_close_cb(nsqBufdSock *buffsock, void *arg)
{
    nsqdConn *conn = (nsqdConn *)arg;

    _DEBUG("%s: %p\n", __FUNCTION__, arg);

    if (conn->close_callback) {
        conn->close_callback(conn, conn->arg);
    }
}

static void nsqd_connection_error_cb(nsqBufdSock *buffsock, void *arg)
{
    nsqdConn *conn = (nsqdConn *)arg;

    _DEBUG("%s: conn %p\n", __FUNCTION__, conn);
}

nsqdConn *new_nsqd_connection(struct ev_loop *loop, const char *address, int port,
    void (*connect_callback)(nsqdConn *conn, void *arg),
    void (*close_callback)(nsqdConn *conn, void *arg),
    void (*msg_callback)(nsqdConn *conn, nsqMsg *msg, void *arg),
    void *arg)
{
    nsqio *nio = (nsqio *)arg;
    nsqdConn *conn;

    conn = (nsqdConn *)malloc(sizeof(nsqdConn));
    conn->address = strdup(address);
    conn->port = port;
    conn->command_buf = new_buffer(nio->cfg->command_buf_len, nio->cfg->command_buf_capacity);
    conn->current_msg_size = 0;
    conn->connect_callback = connect_callback;
    conn->close_callback = close_callback;
    conn->msg_callback = msg_callback;
    conn->arg = arg;
    conn->loop = loop;
    conn->reconnect_timer = NULL;

    conn->bs = new_buffered_socket(loop, address, port,
        nio->cfg->read_buf_len, nio->cfg->read_buf_capacity,
        nio->cfg->write_buf_len, nio->cfg->write_buf_capacity,
        nsqd_connection_connect_cb, nsqd_connection_close_cb,
        NULL, NULL, nsqd_connection_error_cb,
        conn);

    return conn;
}

void free_nsqd_connection(nsqdConn *conn)
{
    if (conn) {
        nsqd_connection_stop_timer(conn);
        free(conn->address);
        free_buffer(conn->command_buf);
        free_buffered_socket(conn->bs);
        free(conn);
    }
}

int nsqd_connection_connect(nsqdConn *conn)
{
    return buffered_socket_connect(conn->bs);
}

void nsqd_connection_disconnect(nsqdConn *conn)
{
    buffered_socket_close(conn->bs);
}

void nsqd_connection_init_timer(nsqdConn *conn,
        void (*reconnect_callback)(EV_P_ ev_timer *w, int revents))
{
    nsqio *nio = (nsqio *)conn->arg;
    conn->reconnect_timer = (ev_timer *)malloc(sizeof(ev_timer));
    ev_timer_init(conn->reconnect_timer, reconnect_callback, nio->cfg->lookupd_interval, nio->cfg->lookupd_interval);
    conn->reconnect_timer->data = conn;
}

void nsqd_connection_stop_timer(nsqdConn *conn)
{
    if (conn && conn->reconnect_timer) {
        ev_timer_stop(conn->loop, conn->reconnect_timer);
        free(conn->reconnect_timer);
    }
}
