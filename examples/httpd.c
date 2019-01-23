#include <stdio.h>
#include "bee.h"
#include "bee_http.h"

void test_cb(int sfd, bh_request_t *request)
{
    bh_send_reply(sfd, "text/plain", "Hello, World!\n", 14);
}

int main(int argc, char **argv)
{
    struct event_base *evbase = event_base_new();
    bee_server_t *server = bh_server_new(evbase, "0.0.0.0", 8000, -1);

    bh_server_set_cb(server, "/", test_cb);
    bh_server_set_cb(server, "/hello", test_cb);
    printf("Start http server with port 8000\n");
    event_base_loop(evbase, 0);
    bh_server_free(server);
    event_base_free(evbase);

    return 0;
}

