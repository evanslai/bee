#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "bee.h"

#define BUF_SIZE 4096

enum BEE_HOOK_RESULT udp_echo_recv(int sfd, void *arg)
{
    bee_server_t * server = arg;
    char buf[BUF_SIZE];
    ssize_t recv_nr, send_nr;
    struct sockaddr_in cli_sock;
    socklen_t cli_len = sizeof(cli_sock);

    memset(buf, 0, sizeof(buf));
    recv_nr = recvfrom(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&cli_sock, &cli_len);
    if (recv_nr < 0) {
        if (errno == EAGAIN)
            return BEE_HOOK_EAGAIN;
        perror("recvfrom");
        return BEE_HOOK_ERR;
    }

    send_nr = sendto(sfd, buf, (size_t)recv_nr, 0, (struct sockaddr *)&cli_sock, cli_len);
    if (send_nr < 0) {
        perror("send");
        return BEE_HOOK_ERR;
    }

    return BEE_HOOK_OK;
}

int main(int argc, char **argv)
{
    struct event_base *evbase = event_base_new();
    bee_server_t *server = bee_server_udp_new(evbase, "0.0.0.0", 8000);

    server->on_recv = udp_echo_recv;
    printf("Start udp echo server with port 8000\n");
    event_base_loop(evbase, 0);
    bee_server_free(server);
    event_base_free(evbase);

    return 0;
}

