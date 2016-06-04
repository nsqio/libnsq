#include <json-c/json.h>
#include "nsq.h"
#include "http.h"

#ifdef DEBUG
#define _DEBUG(...) fprintf(stdout, __VA_ARGS__)
#else
#define _DEBUG(...) do {;} while (0)
#endif

void nsq_lookupd_request_cb(struct HttpRequest *req, struct HttpResponse *resp, void *arg)
{
    struct NSQReader *rdr = (struct NSQReader *)arg;
    struct json_object *jsobj, *data, *producers, *producer, *broadcast_address_obj, *tcp_port_obj;
    struct json_tokener *jstok;
    struct NSQDConnection *conn;
    const char *broadcast_address;
    int i, found, tcp_port;

    _DEBUG("%s: status_code %d, body %.*s\n", __FUNCTION__, resp->status_code,
        (int)BUFFER_HAS_DATA(resp->data), resp->data->data);

    if (resp->status_code != 200) {
        free_http_response(resp);
        free_http_request(req);
        return;
    }

    jstok = json_tokener_new();
    jsobj = json_tokener_parse_ex(jstok, resp->data->data, (int)BUFFER_HAS_DATA(resp->data));
    if (!jsobj) {
        _DEBUG("%s: error parsing JSON\n", __FUNCTION__);
        json_tokener_free(jstok);
        return;
    }

    json_object_object_get_ex(jsobj, "data", &data);
    if (!data) {
        _DEBUG("%s: error getting 'data' key\n", __FUNCTION__);
        json_object_put(jsobj);
        json_tokener_free(jstok);
        return;
    }
    json_object_object_get_ex(data, "producers", &producers);
    if (!producers) {
        _DEBUG("%s: error getting 'producers' key\n", __FUNCTION__);
        json_object_put(jsobj);
        json_tokener_free(jstok);
        return;
    }

    _DEBUG("%s: num producers %d\n", __FUNCTION__, json_object_array_length(producers));
    for (i = 0; i < json_object_array_length(producers); i++) {
        producer = json_object_array_get_idx(producers, i);
        json_object_object_get_ex(producer, "broadcast_address", &broadcast_address_obj);
        json_object_object_get_ex(producer, "tcp_port", &tcp_port_obj);

        broadcast_address = json_object_get_string(broadcast_address_obj);
        tcp_port = json_object_get_int(tcp_port_obj);

        _DEBUG("%s: broadcast_address %s, port %d\n", __FUNCTION__, broadcast_address, tcp_port);

        found = 0;
        LL_FOREACH(rdr->conns, conn) {
            if (strcmp(conn->bs->address, broadcast_address) == 0
                && conn->bs->port == tcp_port) {
                found = 1;
                break;
            }
        }

        if (!found) {
            nsq_reader_connect_to_nsqd(rdr, broadcast_address, tcp_port);
        }
    }

    json_object_put(jsobj);
    json_tokener_free(jstok);

    free_http_response(resp);
    free_http_request(req);
}

struct NSQLookupdEndpoint *new_nsqlookupd_endpoint(const char *address, int port)
{
    struct NSQLookupdEndpoint *nsqlookupd_endpoint;

    nsqlookupd_endpoint = (struct NSQLookupdEndpoint *)malloc(sizeof(struct NSQLookupdEndpoint));
    nsqlookupd_endpoint->address = strdup(address);
    nsqlookupd_endpoint->port = port;
    nsqlookupd_endpoint->next = NULL;

    return nsqlookupd_endpoint;
}

void free_nsqlookupd_endpoint(struct NSQLookupdEndpoint *nsqlookupd_endpoint)
{
    if (nsqlookupd_endpoint) {
        free(nsqlookupd_endpoint->address);
        free(nsqlookupd_endpoint);
    }
}
