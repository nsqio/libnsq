#include "http.h"
#include "json.h"
#include "nsq.h"
#include "utlist.h"

void nsq_lookupd_request_cb(httpRequest *req, httpResponse *resp, void *arg)
{
    nsqio *rdr = (nsqio *)arg;
    nsq_json_t *jsobj, *data, *producers, *producer, *broadcast_address_obj, *tcp_port_obj;
    nsq_json_tokener_t *jstok;
    nsqdConn *conn;
    const char *broadcast_address;
    int i, found, tcp_port;

    _DEBUG("%s: status_code %d, body %.*s\n", __FUNCTION__, resp->status_code,
        (int)BUFFER_HAS_DATA(resp->data), resp->data->data);

    if (resp->status_code != 200) {
        free_http_response(resp);
        free_http_request(req);
        return;
    }

    jstok = nsq_json_tokener_new();
    jsobj = nsq_json_loadb(resp->data->data, (nsq_json_size_t)BUFFER_HAS_DATA(resp->data), 0, jstok);
    if (!jsobj) {
        _DEBUG("%s: error parsing JSON\n", __FUNCTION__);
        nsq_json_tokener_free(jstok);
        return;
    }

    nsq_json_object_get(jsobj, "data", &data);
    if (!data) {
        _DEBUG("%s: error getting 'data' key\n", __FUNCTION__);
        nsq_json_decref(jsobj);
        nsq_json_tokener_free(jstok);
        return;
    }
    nsq_json_object_get(data, "producers", &producers);
    if (!producers) {
        _DEBUG("%s: error getting 'producers' key\n", __FUNCTION__);
        nsq_json_decref(jsobj);
        nsq_json_tokener_free(jstok);
        return;
    }

    _DEBUG("%s: num producers %ld\n", __FUNCTION__, (long)nsq_json_array_length(producers));
    for (i = 0; i < nsq_json_array_length(producers); i++) {
        producer = nsq_json_array_get(producers, i);
        nsq_json_object_get(producer, "broadcast_address", &broadcast_address_obj);
        nsq_json_object_get(producer, "tcp_port", &tcp_port_obj);

        broadcast_address = nsq_json_string_value(broadcast_address_obj);
        tcp_port = nsq_json_int_value(tcp_port_obj);

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

    nsq_json_decref(jsobj);
    nsq_json_tokener_free(jstok);

    free_http_response(resp);
    free_http_request(req);
}

nsqLookupdEndpoint *new_nsqlookupd_endpoint(const char *address, int port)
{
    nsqLookupdEndpoint *nsqlookupd_endpoint;

    nsqlookupd_endpoint = (nsqLookupdEndpoint *)malloc(sizeof(nsqLookupdEndpoint));
    nsqlookupd_endpoint->address = strdup(address);
    nsqlookupd_endpoint->port = port;
    nsqlookupd_endpoint->next = NULL;

    return nsqlookupd_endpoint;
}

void free_nsqlookupd_endpoint(nsqLookupdEndpoint *nsqlookupd_endpoint)
{
    if (nsqlookupd_endpoint) {
        free(nsqlookupd_endpoint->address);
        free(nsqlookupd_endpoint);
    }
}
