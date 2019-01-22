#include <stdio.h>
#include "bee.h"
#include "bee_cli.h"

void test_cb(int sfd, int argc, char **argv)
{
    int i;

    for (i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }
    printf("\n\n");

    bcli_println(sfd, "Usage: %s [OPTION]...", argv[0]);
}

int main(int argc, char **argv)
{
    struct event_base *evbase = event_base_new();
    bee_server_t *server = bcli_server_new(evbase, "0.0.0.0", 8000, -1);

    bcli_server_set_cb(server, "test", test_cb);
    printf("Start cli server with port 8000\n");
    event_base_loop(evbase, 0);
    bcli_server_free(server);
    event_base_free(evbase);

    return 0;
}

