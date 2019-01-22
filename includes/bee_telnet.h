#ifndef __BEE_TELNET_H__
#define __BEE_TELNET_H__
#include <sys/queue.h>
#include "bee.h"

struct bee_telnet_callback;
struct bee_telnet;

typedef struct bee_telnet_callback      bee_telnet_callback_t;
typedef struct bee_telnet               bee_telnet_t;

typedef void (* bee_telnet_callback_cb)(int sfd, int argc, char **argv);


struct bee_telnet_callback {
    char                              * path;
    bee_telnet_callback_cb              cb;
    TAILQ_ENTRY(bee_telnet_callback)    next;
};

struct bee_telnet {
    TAILQ_HEAD(, bee_telnet_callback)   callbacks;
};


bee_server_t * bee_telnetd_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog);
void bee_telnetd_free(bee_server_t *server);
bee_telnet_callback_t * bee_telnetd_set_cb(bee_server_t *server, const char *path, bee_telnet_callback_cb cb);

void bee_telnetd_prompt(int sfd, char *str);
void bee_telnetd_println(int sfd, const char *fmt, ...);


#endif
