#include "nsq.h"

struct NSQLookupdEndpoint *new_nsqlookupd_endpoint(const char *address, int port)
{
    struct NSQLookupdEndpoint *nsqlookupd_endpoint;
    
    nsqlookupd_endpoint = malloc(sizeof(struct NSQLookupdEndpoint));
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
