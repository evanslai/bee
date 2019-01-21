#ifndef __BEE_H__
#define __BEE_H__
#include <event2/event.h>

enum BEE_SERVER_TYPE {
    BEE_SERVER_TCP,
    BEE_SERVER_UDP,
    BEE_SERVER_MCAST_UDP
};

enum BEE_HOOK_RESULT {
    BEE_HOOK_OK,
    BEE_HOOK_PEER_CLOSED,
    BEE_HOOK_EAGAIN,
    BEE_HOOK_ERR = -1
};


struct bee_server;
struct bee_connection;

typedef struct bee_server            bee_server_t;
typedef struct bee_connection        bee_connection_t;

/* If the server type is TCP, the `arg' is bee_connection_t structure.
 * If the server type is UDP or MCAST_UDP, the `arg' is bee_server_t structure.
 */
typedef enum BEE_HOOK_RESULT (* bee_server_hook_t)(int sfd, void * arg);

struct bee_server {
    enum BEE_SERVER_TYPE        type;
    struct event_base         * evbase;
    struct event              * listen_ev;
    bee_server_hook_t           on_accept;
    bee_server_hook_t           on_recv;
    void                      * pdata;      /* user-defined data */
};

/* only for tcp connection */
struct bee_connection {
    bee_server_t              * server;
    struct event              * accept_ev;
    struct sockaddr             saddr;      /* the client come from where */
    void                      * pdata;      /* user-defined data */
};


/* bee.c */
bee_server_t * bee_server_tcp_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog);
bee_server_t * bee_server_udp_new(struct event_base *evbase, const char *baddr, uint16_t port);
bee_server_t * bee_server_mcast_new(struct event_base *evbase, const char *laddr, const char *gaddr, uint16_t port);
void bee_server_free(bee_server_t *server);


#endif
