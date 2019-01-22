#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "bee.h"
#include "bee_http.h"

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)          \
    for ((var) = TAILQ_FIRST((head));                       \
         (var) && ((tvar) = TAILQ_NEXT((var), field), 1);   \
         (var) = (tvar))
#endif


#define NOTFOUND_RESPONSE       \
    "HTTP/1.1 404 Not Found\r\n"    \
    "Content-Type: text/plain\r\n"  \
    "Content-Length: 48\r\n"        \
    "\r\n"                          \
    "The requested URL was not found on this server.\n"


static bh_request_t *
__http_request_new(void)
{
    bh_request_t *request;
    bh_header_t *header = NULL;
    int i;

    request = malloc(sizeof(bh_request_t));
    assert(request != NULL);

    request->url = NULL;
    request->method = NULL;
    request->header_lines = 0;

    for (i = 0; i < MAX_HTTP_HEADERS; i++) {
        header = &request->headers[i];
        header->field = NULL;
        header->value = NULL;
        header->field_len = 0;
        header->value_len = 0;
    }

    request->body = NULL;
    request->body_len = 0;

    return request;
}

static void
__http_request_free(bh_request_t *request)
{
    int i;
    bh_header_t *header = NULL;

    if (request->url != NULL) {
        free(request->url);
        request->url = NULL;
    }

    if (request->method != NULL) {
        free(request->method);
        request->method = NULL;
    }

    for (i = 0; i < request->header_lines; ++i) {
        header = &request->headers[i];
        if (header->field != NULL) {
            free(header->field);
            header->field = NULL;
            header->field_len = 0;
        }
        if (header->value != NULL) {
            free(header->value);
            header->value = NULL;
            header->value_len = 0;
        }
    }

    request->header_lines = 0;

    if (request->body != NULL) {
        free(request->body);
        request->body = NULL;
        request->body_len = 0;
    }

    free(request);
}

/*---------------------------------------------------------------------------*/
/* http_parser callbacks                                                     */
/*---------------------------------------------------------------------------*/
static int
__on_message_begin(http_parser *parser)
{
    return 0;
}

static int
__on_headers_complete(http_parser *parser)
{
    size_t len;
    bh_request_t *request = parser->data;
    const char *method = http_method_str((enum http_method)parser->method);

    len = strlen(method);
    request->method = malloc(len + 1);
    assert(request->method != NULL);
    strncpy(request->method, method, len);
    request->method[len] = '\0';

    return 0;
}

static int
__on_url(http_parser *parser, const char *at, size_t len)
{
    bh_request_t *request = parser->data;

    request->url = malloc(len + 1);
    assert(request->url != NULL);
    strncpy(request->url, at, len);
    request->url[len] = '\0';
    return 0;
}

static int
__on_header_field(http_parser *parser, const char *at, size_t len)
{
    bh_request_t *request = parser->data;
    bh_header_t *header = &request->headers[request->header_lines];

    header->field = malloc(len + 1);
    assert(header->field != NULL);
    header->field_len = len;
    strncpy(header->field, at, len);
    header->field[len] = '\0';
    return 0;
}

static int
__on_header_value(http_parser *parser, const char *at, size_t len)
{
    bh_request_t *request = parser->data;
    bh_header_t *header = &request->headers[request->header_lines];

    header->value = malloc(len + 1);
    assert(header->value != NULL);
    header->value_len = len;
    strncpy(header->value, at, len);
    header->value[len] = '\0';
    ++request->header_lines;

    return 0;
}

static int
__on_body(http_parser *parser, const char *at, size_t len)
{
    bh_request_t *request = parser->data;

    request->body = malloc(len + 1);
    assert(request->body != NULL);
    request->body_len = len;
    strncpy(request->body, at, len);
    request->body[len] = '\0';

    return 0;
}

static int
__on_message_complete(http_parser *parser)
{
    bh_request_t *request = parser->data;
    bh_header_t *header = NULL;
    int i;

    printf("url: %s\n", request->url);
    printf("method: %s\n", request->method);
    for (i = 0; i < MAX_HTTP_HEADERS; ++i) {
        header = &request->headers[i];
        if (header->field) {
            printf("header: %s: %s\n", header->field, header->value);
        }
    }
    printf("body: %s\n", request->body);
    printf("\n\n");

    return 0;
}
/*---------------------------------------------------------------------------*/




/*---------------------------------------------------------------------------*/
/* Bee server callbacks                                                      */
/*---------------------------------------------------------------------------*/
enum BEE_HOOK_RESULT http_recv(int sfd, void *arg)
{
    bee_connection_t *conn = arg;
    bh_server_t *httpd = conn->server->pdata;
    bh_request_t *request;
    http_parser parser;
    char buf[65535];
    ssize_t nparsed = 0, nr = 0;

    memset(buf, 0, sizeof(buf));
    nr = recv(sfd, buf, sizeof(buf), 0);
    if (nr < 0) {
        if (errno == EAGAIN)
            return BEE_HOOK_EAGAIN;
        perror("recv");
        return BEE_HOOK_ERR;
    }
    else if (nr == 0) {
        close(sfd);
        return BEE_HOOK_PEER_CLOSED;
    }

    request = __http_request_new();
    if (request == NULL)
        return BEE_HOOK_ERR;

    http_parser_init(&parser, HTTP_REQUEST);
    parser.data = request;
    nparsed = http_parser_execute(&parser, &httpd->parser_settings, buf, nr);
    if (nparsed < nr)
        fprintf(stderr, "parse error.\n");
    else {
        /* handle the http request */
        bh_callback_t *callback;
        int found = 0;

        TAILQ_FOREACH(callback, &httpd->callbacks, next) {
            if (strcmp(callback->path, request->url) == 0) {
                callback->cb(sfd, request);
                found = 1;
                break;
            }
        }

        if (!found) {
            nr = send(sfd, NOTFOUND_RESPONSE, sizeof(NOTFOUND_RESPONSE), 0);
            if (nr < 0) {
                perror("send");
                return BEE_HOOK_ERR;
            }
        }
    }

    __http_request_free(request);
    return BEE_HOOK_CLOSED;
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Exported functions                                                        */
/*---------------------------------------------------------------------------*/
bee_server_t *
bh_server_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog)
{
    bh_server_t * httpd;
    bee_server_t * server;

    httpd = calloc(1, sizeof(*httpd));
    if (!httpd)
        return NULL;

    httpd->parser_settings.on_message_begin = __on_message_begin;
    httpd->parser_settings.on_url = __on_url;
    httpd->parser_settings.on_header_field = __on_header_field;
    httpd->parser_settings.on_header_value = __on_header_value;
    httpd->parser_settings.on_headers_complete = __on_headers_complete;
    httpd->parser_settings.on_body = __on_body;
    httpd->parser_settings.on_message_complete = __on_message_complete;
    TAILQ_INIT(&httpd->callbacks);

    server = bee_server_tcp_new(evbase, baddr, port, backlog);
    if (!server) {
        free(httpd);
        return NULL;
    }

    server->pdata = httpd;
    server->on_recv = http_recv;

    return server;
}

void
bh_server_free(bee_server_t *server)
{
    bh_server_t *httpd = server->pdata;
    bh_callback_t *callback, *tmp;

    TAILQ_FOREACH_SAFE(callback, &httpd->callbacks, next, tmp) {
        if (callback->path != NULL)
            free(callback->path);

        TAILQ_REMOVE(&httpd->callbacks, callback, next);
        free(callback);
    }

    free(httpd);
    bee_server_free(server);
}


bh_callback_t *
bh_server_set_cb(bee_server_t *server, const char *path, bh_callback_cb cb)
{
    bh_server_t *httpd = server->pdata;
    bh_callback_t *callback;

    callback = calloc(1, sizeof(*callback));
    if (!callback)
        return NULL;

    callback->path = strdup(path);
    callback->cb = cb;
    TAILQ_INSERT_TAIL(&httpd->callbacks, callback, next);

    return callback;
}

