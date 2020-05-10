#include <string.h>

#include "libevbuffsock/evbuffsock.h"
#include "http.h"

static void timer_cb(EV_P_ struct ev_timer *w, int revents);

static int multi_timer_cb(CURLM *multi, long timeout_ms, void *arg)
{
    httpClient *httpc = (httpClient *)arg;

    ev_timer_stop(httpc->loop, httpc->timer_event);
    if (timeout_ms > 0) {
        double t = timeout_ms / 1000;
        ev_timer_init(httpc->timer_event, timer_cb, t, 0.);
        ev_timer_start(httpc->loop, httpc->timer_event);
    } else {
        timer_cb(httpc->loop, httpc->timer_event, 0);
    }

    return 0;
}

static void check_multi_info(httpClient *httpc)
{
    char *eff_url;
    CURLMsg *msg;
    int msgs_left;
    int status_code;
    httpRequest *req;
    httpResponse *resp;
    CURL *easy;

    while ((msg = curl_multi_info_read(httpc->multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            easy = msg->easy_handle;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &req);
            curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &eff_url);
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status_code);
            if (req->callback) {
                resp = new_http_response(status_code, req->data);
                req->callback(req, resp, req->cb_arg);
            }
        }
    }
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *arg)
{
    httpRequest *req = (httpRequest *)arg;
    size_t realsize = size * nmemb;

    buffer_add(req->data, ptr, realsize);

    return realsize;
}

static void event_cb(EV_P_ struct ev_io *w, int revents)
{
    httpClient *httpc = (httpClient *)w->data;
    CURLMcode rc;
    int action = (revents & EV_READ ? CURL_POLL_IN : 0) | (revents & EV_WRITE ? CURL_POLL_OUT : 0);

    rc = curl_multi_socket_action(httpc->multi, w->fd, action, &httpc->still_running);
    if (rc != CURLM_OK) {
        // TODO: handle rc
    }
    check_multi_info(httpc);
}

static void timer_cb(EV_P_ struct ev_timer *w, int revents)
{
    httpClient *httpc = (httpClient *)w->data;
    CURLMcode rc;

    rc = curl_multi_socket_action(httpc->multi, CURL_SOCKET_TIMEOUT, 0, &httpc->still_running);
    if (rc != CURLM_OK) {
        // TODO: handle rc
    }
    check_multi_info(httpc);
}

static int sock_cb(CURL *e, curl_socket_t s, int what, void *arg, void *sock_arg)
{
    httpClient *httpc = (httpClient *)arg;
    httpSocket *sock = (httpSocket *)sock_arg;
    int kind = (what & CURL_POLL_IN ? EV_READ : 0) | (what & CURL_POLL_OUT ? EV_WRITE : 0);

    if (what == CURL_POLL_REMOVE) {
        if (sock) {
            if (sock->evset) {
                ev_io_stop(httpc->loop, sock->ev);
                free(sock->ev);
            }
            free(sock);
        }
    } else {
        int with_new = 0;
        if (!sock) {
            sock = (httpSocket *)calloc(1, sizeof(httpSocket));
            sock->ev = malloc(sizeof(struct ev_io));
            with_new = 1;
        }

        sock->httpc = httpc;
        sock->sockfd = s;
        sock->action = what;
        sock->easy = e;
        if (sock->evset) {
            ev_io_stop(httpc->loop, sock->ev);
            free(sock->ev);
        }
        ev_io_init(sock->ev, event_cb, sock->sockfd, kind);
        sock->ev->data = httpc;
        sock->evset = 1;
        ev_io_start(httpc->loop, sock->ev);

        if (with_new) {
            curl_multi_assign(httpc->multi, s, sock);
        }
    }

    return 0;
}

httpClient *new_http_client(struct ev_loop *loop)
{
    httpClient *httpc;

    httpc = (httpClient *)malloc(sizeof(httpClient));
    httpc->loop = loop;
    httpc->multi = curl_multi_init();
    httpc->timer_event = malloc(sizeof(struct ev_timer));
    ev_timer_init(httpc->timer_event, timer_cb, 0., 0.);
    httpc->timer_event->data = httpc;
    curl_multi_setopt(httpc->multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
    curl_multi_setopt(httpc->multi, CURLMOPT_SOCKETDATA, httpc);
    curl_multi_setopt(httpc->multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
    curl_multi_setopt(httpc->multi, CURLMOPT_TIMERDATA, httpc);

    return httpc;
}

void free_http_client(httpClient *httpc)
{
    if (httpc) {
        curl_multi_cleanup(httpc->multi);
        ev_timer_stop(httpc->loop, httpc->timer_event);
        free(httpc->timer_event);
        free(httpc);
    }
}

httpRequest *new_http_request(const char *url,
    void (*callback)(httpRequest *req, httpResponse *resp, void *arg), void *cb_arg)
{
    httpRequest *req;

    req = (httpRequest *)calloc(1, sizeof(httpRequest));
    req->data = new_buffer(4096, 0);
    req->easy = curl_easy_init();
    if (!req->easy) {
        return 0;
    }
    req->url = strdup(url);
    req->callback = callback;
    req->cb_arg = cb_arg;

    curl_easy_setopt(req->easy, CURLOPT_URL, req->url);
    curl_easy_setopt(req->easy, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(req->easy, CURLOPT_WRITEDATA, req);
    curl_easy_setopt(req->easy, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(req->easy, CURLOPT_ERRORBUFFER, req->error);
    curl_easy_setopt(req->easy, CURLOPT_PRIVATE, req);
    curl_easy_setopt(req->easy, CURLOPT_NOSIGNAL, 1L);

    return req;
}

void free_http_request(httpRequest *req)
{
    if (req) {
        curl_multi_remove_handle(req->httpc->multi, req->easy);
        curl_easy_cleanup(req->easy);
        free_buffer(req->data);
        free(req->url);
        free(req);
    }
}

httpResponse *new_http_response(int status_code, void *data)
{
    httpResponse *resp;

    resp = (httpResponse *)malloc(sizeof(httpResponse));
    resp->status_code = status_code;
    resp->data = data;

    return resp;
}

void free_http_response(httpResponse *resp)
{
    if (resp) {
        free(resp);
    }
}

int http_client_get(httpClient *httpc, httpRequest *req)
{
    CURLMcode rc;

    req->httpc = httpc;
    rc = curl_multi_add_handle(httpc->multi, req->easy);
    if (rc != CURLM_OK) {
        return 0;
    }

    return 1;
}
