#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "bee.h"
#include "bee_telnet.h"


void test_cb(int sfd, int argc, char **argv)
{
    int i;

    for (i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }
    printf("\n\n");

    bee_telnetd_println(sfd, "Usage: %s [OPTION]...", argv[0]);
}


int main(int argc, char **argv)
{
    struct event_base *evbase = event_base_new();
    bee_server_t *server = bee_telnetd_new(evbase, "0.0.0.0", 8000, -1);

    bee_telnetd_set_cb(server, "test", test_cb);
    printf("Start http server with port 8000\n");
    event_base_loop(evbase, 0);
    bee_telnetd_free(server);
    event_base_free(evbase);

    return 0;
}

