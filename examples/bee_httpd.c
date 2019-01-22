#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "bee.h"
#include "bee_http.h"


#define RESPONSE                    \
    "HTTP/1.1 200 OK\r\n"           \
    "Content-Type: text/plain\r\n"  \
    "Content-Length: 14\r\n"        \
    "\r\n"                          \
    "Hello, World!\n"


void bhttpd_root_cb(int sfd, bee_http_request_t *req)
{
    ssize_t nr = 0;
    nr = send(sfd, RESPONSE, sizeof(RESPONSE), 0);
    if (nr < 0) {
        perror("send");
    }
}

int main(int argc, char **argv)
{
    struct event_base *evbase = event_base_new();
    bee_server_t *server = bee_httpd_new(evbase, "0.0.0.0", 8000, -1);

    bee_httpd_set_cb(server, "/", bhttpd_root_cb);
    bee_httpd_set_cb(server, "/hello", bhttpd_root_cb);
    printf("Start http server with port 8000\n");
    event_base_loop(evbase, 0);
    bee_httpd_free(server);
    event_base_free(evbase);

    return 0;
}

