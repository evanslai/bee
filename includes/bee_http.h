#ifndef __BEE_HTTP_H__
#define __BEE_HTTP_H__
#include <sys/queue.h>
#include "bee.h"
#include "http_parser.h"

#define MAX_HTTP_HEADERS        (128)


struct bee_http_header;
struct bee_http_request;
struct bee_http_callback;
struct bee_http;


typedef struct bee_http_header      bee_http_header_t;
typedef struct bee_http_request     bee_http_request_t;
typedef struct bee_http_callback    bee_http_callback_t;
typedef struct bee_http             bee_http_t;


typedef void (* bee_http_callback_cb)(int sfd, bee_http_request_t *req);


struct bee_http_header {
    char      * field;
    char      * value;
    size_t      field_length;
    size_t      value_length;
};

struct bee_http_request {
    char              * url;
    char              * method;
    int                 header_lines;
    bee_http_header_t   headers[MAX_HTTP_HEADERS];
    char              * body;
    size_t              body_length;
};

struct bee_http_callback {
    char                          * path;
    bee_http_callback_cb            cb;
    TAILQ_ENTRY(bee_http_callback)  next;
};

struct bee_http {
    http_parser_settings                parser_settings;
    TAILQ_HEAD(, bee_http_callback)     callbacks;
};


bee_server_t * bee_httpd_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog);
void bee_httpd_free(bee_server_t *server);
bee_http_callback_t * bee_httpd_set_cb(bee_server_t *server, const char *path, bee_http_callback_cb cb);

#endif

