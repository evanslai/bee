# Bee
## About
Bee is an event-based server skeleton written in C.

## Building
```
git clone https://github.com/evanslai/bee.git
cd bee
mkdir build
cd build
cmake ..
make
```

## TCP Echo Server Example
```
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "bee.h"

#define BUF_SIZE 4096

enum BEE_HOOK_RESULT tcp_echo_recv(int sfd, void * arg)
{
    bee_connection_t * conn = arg;
    char buf[BUF_SIZE];
    ssize_t recv_nr, send_nr;

    memset(buf, 0, sizeof(buf));
    recv_nr = recv(sfd, buf, sizeof(buf), 0);
    if (recv_nr < 0) {
        if (errno == EAGAIN)
            return BEE_HOOK_EAGAIN;
        perror("recv");
        return BEE_HOOK_ERR;
    }
    else if (recv_nr == 0)
        return BEE_HOOK_PEER_CLOSED;

    send_nr = send(sfd, buf, (size_t)recv_nr, 0);
    if (send_nr < 0) {
        perror("send");
        return BEE_HOOK_ERR;
    }

    return BEE_HOOK_OK;
}

int main(int argc, char **argv)
{
    struct event_base *evbase = event_base_new();
    bee_server_t *server = bee_server_tcp_new(evbase, "0.0.0.0", 8000, -1);

    server->on_recv = tcp_echo_recv;
    printf("Start tcp echo server with port 8000\n");
    event_base_loop(evbase, 0);
    bee_server_free(server);
    event_base_free(evbase);

    return 0;
}
```

Please refer to sample codes in the examples directory for more details.
