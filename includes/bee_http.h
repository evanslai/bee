#ifndef __BEE_HTTP_H__
#define __BEE_HTTP_H__
#include <sys/queue.h>
#include "bee.h"
#include "http_parser.h"

#define MAX_HTTP_HEADERS        (128)


struct bh_header;
struct bh_request;
struct bh_callback;
struct bh_server;


typedef struct bh_header      bh_header_t;
typedef struct bh_request     bh_request_t;
typedef struct bh_callback    bh_callback_t;
typedef struct bh_server      bh_server_t;


typedef void (* bh_callback_cb)(int sfd, bh_request_t *req);


struct bh_header {
    char      * field;
    char      * value;
    size_t      field_len;
    size_t      value_len;
};

struct bh_request {
    char              * url;
    char              * method;
    int                 header_lines;
    bh_header_t         headers[MAX_HTTP_HEADERS];
    char              * body;
    size_t              body_len;
};

struct bh_callback {
    char                      * path;
    bh_callback_cb              cb;
    TAILQ_ENTRY(bh_callback)    next;
};

struct bh_server {
    http_parser_settings          parser_settings;
    TAILQ_HEAD(, bh_callback)     callbacks;
};


bee_server_t * bh_server_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog);
void bh_server_free(bee_server_t *server);
bh_callback_t * bh_server_set_cb(bee_server_t *server, const char *path, bh_callback_cb cb);

void bh_send_reply(int sfd, const char *content_type, const char *body, int body_len);
#endif

