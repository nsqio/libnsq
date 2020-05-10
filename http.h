#ifndef __http_h
#define __http_h

#include <ev.h>
#include <curl/curl.h>

typedef struct HttpClient {
    CURLM *multi;
    struct ev_loop *loop;
    struct ev_timer *timer_event;
    int still_running;
} httpClient;

typedef struct HttpResponse {
    int status_code;
    struct Buffer *data;
} httpResponse;

typedef struct HttpRequest {
    CURL *easy;
    char *url;
    struct HttpClient *httpc;
    char error[CURL_ERROR_SIZE];
    struct Buffer *data;
    void (*callback)(struct HttpRequest *req, struct HttpResponse *resp, void *arg);
    void *cb_arg;
} httpRequest;

typedef struct HttpSocket {
    curl_socket_t sockfd;
    CURL *easy;
    int action;
    long timeout;
    struct ev_io *ev;
    int evset;
    struct HttpClient *httpc;
} httpSocket;

httpClient *new_http_client(struct ev_loop *loop);
void free_http_client(httpClient *httpc);
httpRequest *new_http_request(const char *url,
    void (*callback)(httpRequest *req, httpResponse *resp, void *arg), void *cb_arg);
void free_http_request(httpRequest *req);
httpResponse *new_http_response(int status_code, void *data);
void free_http_response(httpResponse *resp);
int http_client_get(httpClient *httpc, httpRequest *req);

#endif
