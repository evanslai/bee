#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include "bee.h"


static void
__udp_conn_read_cb(evutil_socket_t sfd, short events, void *arg)
{
    bee_server_t *server = arg;
    enum BEE_HOOK_RESULT status;

    if (server->on_recv != NULL)
        status = server->on_recv(sfd, server);

    if (status == BEE_HOOK_ERR)
        event_base_loopexit(server->evbase, NULL);

    return;
}


static void
__tcp_conn_read_cb(evutil_socket_t sfd, short events, void *arg)
{
    bee_connection_t *conn = arg;
    bee_server_t *server = conn->server;
    enum BEE_HOOK_RESULT status;

    if (server->on_recv != NULL)
        status = server->on_recv(sfd, conn);

    if (status == BEE_HOOK_PEER_CLOSED || status == BEE_HOOK_ERR) {
        conn->server = NULL;
        event_free(conn->accept_ev);
        free(conn);
        close(sfd);
    }

    if (status == BEE_HOOK_ERR)
        event_base_loopexit(server->evbase, NULL);

    return;
}


static void
__tcp_conn_accept_cb(evutil_socket_t sfd, short events, void *arg)
{
    bee_server_t *server = arg;
    bee_connection_t *conn;
    evutil_socket_t cli_sfd;
    struct sockaddr_in cli_sock;
    socklen_t cli_len = sizeof(cli_sock);

    cli_sfd = accept(sfd, (struct sockaddr *)&cli_sock, &cli_len);
    if (cli_sfd < 0) {
        perror("accept");
        return;
    }

    if (evutil_make_socket_nonblocking(cli_sfd) < 0)
        goto err;

    conn = calloc(1, sizeof(*conn));
    if (!conn)
        goto err;

    conn->server = server;
    memcpy(&conn->saddr, &cli_sock, cli_len);
    conn->accept_ev = event_new(server->evbase, cli_sfd, EV_READ|EV_PERSIST, __tcp_conn_read_cb, conn);
    if (!conn->accept_ev)
        goto err;

    conn->pdata = NULL;
    event_add(conn->accept_ev, NULL);

    if (server->on_accept != NULL)
        server->on_accept(cli_sfd, conn);

    return;

  err:
    if (conn)
        free(conn);
    close(cli_sfd);
    return;
}


bee_server_t *
bee_server_tcp_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog)
{
    bee_server_t *server = NULL;
    evutil_socket_t sfd;
    struct sockaddr_in lsock;
    int on = 1;

    if (!evbase)
        return NULL;

    if (backlog == 0)
        return NULL;

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
        return NULL;

    if (evutil_make_socket_nonblocking(sfd) < 0)
        goto err;

    if (evutil_make_listen_socket_reuseable(sfd) < 0)
        goto err;

    memset(&lsock, 0, sizeof(lsock));
    lsock.sin_family = AF_INET;
    lsock.sin_addr.s_addr = INADDR_ANY;
    lsock.sin_port = htons(port);

    if (bind(sfd, (struct sockaddr *)&lsock, sizeof(lsock)) < 0)
        goto err;

    if (backlog > 0) {
        if (listen(sfd, backlog) < 0)
            goto err;
    } else if (backlog < 0) {
        if (listen(sfd, 128) < 0)
            goto err;
    }

    server = calloc(1, sizeof(*server));
    if (!server)
        goto err;
    server->evbase = evbase;
    server->type = BEE_SERVER_TCP;
    server->listen_ev = event_new(server->evbase, sfd, EV_READ|EV_PERSIST, __tcp_conn_accept_cb, server);
    if (!server->listen_ev)
        goto err;
    server->on_accept = NULL;
    server->on_recv = NULL;
    server->pdata = NULL;

    event_add(server->listen_ev, NULL);
    return server;

  err:
    if (server != NULL)
        free(server);
    close(sfd);
    return NULL;
}


bee_server_t *
bee_server_udp_new(struct event_base *evbase, const char *baddr, uint16_t port)
{
    bee_server_t *server = NULL;
    evutil_socket_t sfd;
    struct sockaddr_in lsock;

    if (!evbase)
        return NULL;

    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0)
        return NULL;

    if (evutil_make_socket_nonblocking(sfd) < 0)
        goto err;

    if (evutil_make_listen_socket_reuseable(sfd) < 0)
        goto err;

    memset(&lsock, 0, sizeof(lsock));
    lsock.sin_family = AF_INET;
    lsock.sin_addr.s_addr = INADDR_ANY;
    lsock.sin_port = htons(port);

    if (bind(sfd, (struct sockaddr *)&lsock, sizeof(lsock)) < 0)
        goto err;

    server = calloc(1, sizeof(*server));
    if (!server)
        goto err;

    server->evbase = evbase;
    server->type = BEE_SERVER_UDP;
    server->listen_ev = event_new(server->evbase, sfd, EV_READ|EV_PERSIST, __udp_conn_read_cb, server);
    if (!server->listen_ev)
        goto err;
    server->on_accept = NULL;
    server->on_recv = NULL;
    server->pdata = NULL;

    event_add(server->listen_ev, NULL);
    return server;

  err:
    if (server != NULL)
        free(server);
    close(sfd);
    return NULL;
}


bee_server_t *
bee_server_mcast_new(struct event_base *evbase, const char *laddr, const char *gaddr, uint16_t port)
{
    bee_server_t *server = NULL;
    evutil_socket_t sfd;
    struct sockaddr_in lsock;
    char loopch = 1;
    struct ip_mreq group;


    if (!evbase)
        return NULL;

    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0)
        return NULL;

    if (evutil_make_socket_nonblocking(sfd) < 0)
        goto err;

    if (evutil_make_listen_socket_reuseable(sfd) < 0)
        goto err;

    if (setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
        goto err;

    memset(&lsock, 0, sizeof(lsock));
    lsock.sin_family = AF_INET;
    lsock.sin_addr.s_addr = INADDR_ANY;
    lsock.sin_port = htons(port);
    if (bind(sfd, (struct sockaddr *)&lsock, sizeof(lsock)) < 0)
        goto err;

    /* Join the multicast group 'gaddr' on the local 'laddr' interface. */
    group.imr_multiaddr.s_addr = inet_addr(gaddr);
    group.imr_interface.s_addr = inet_addr(laddr);
    if (setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
        goto err;


    server = calloc(1, sizeof(*server));
    if (!server)
        goto err;

    server->evbase = evbase;
    server->type = BEE_SERVER_MCAST_UDP;
    server->listen_ev = event_new(server->evbase, sfd, EV_READ|EV_PERSIST, __udp_conn_read_cb, server);
    if (!server->listen_ev)
        goto err;
    server->on_accept = NULL;
    server->on_recv = NULL;
    server->pdata = NULL;

    event_add(server->listen_ev, NULL);
    return server;

  err:
    if (server != NULL)
        free(server);
    close(sfd);
    return NULL;
}

void
bee_server_free(bee_server_t *server)
{
    evutil_socket_t sfd;

    if (!server)
        return;

    sfd = event_get_fd(server->listen_ev);
    close(sfd);
    event_free(server->listen_ev);
    free(server);
}

