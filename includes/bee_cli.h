#ifndef __BEE_CLI_H__
#define __BEE_CLI_H__
#include <sys/queue.h>
#include "bee.h"

struct bcli_callback;
struct bcli_server;

typedef struct bcli_callback      bcli_callback_t;
typedef struct bcli_server        bcli_server_t;

typedef void (* bcli_callback_cb)(int sfd, int argc, char **argv);


struct bcli_callback {
    char                        * path;
    bcli_callback_cb              cb;
    TAILQ_ENTRY(bcli_callback)    next;
};

struct bcli_server {
    TAILQ_HEAD(, bcli_callback)     callbacks;
};


bee_server_t * bcli_server_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog);
void bcli_server_free(bee_server_t *server);
bcli_callback_t * bcli_server_set_cb(bee_server_t *server, const char *path, bcli_callback_cb cb);
void bcli_set_prompt(const char *prompt);
char * bcli_get_prompt(void);
void bcli_println(int sfd, const char *fmt, ...);
void bcli_prompt(int sfd);


#endif
