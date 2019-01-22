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


static bee_http_request_t *
__http_request_new(void)
{
    bee_http_request_t *http_request;
    bee_http_header_t *header = NULL;
    int i;

    http_request = malloc(sizeof(bee_http_request_t));
    assert(http_request != NULL);

    http_request->url = NULL;
    http_request->method = NULL;
    http_request->header_lines = 0;

    for (i = 0; i < MAX_HTTP_HEADERS; i++) {
        header = &http_request->headers[i];
        header->field = NULL;
        header->value = NULL;
        header->field_length = 0;
        header->value_length = 0;
    }

    http_request->body = NULL;
    http_request->body_length = 0;

    return http_request;
}

static void
__http_request_free(bee_http_request_t *http_request)
{
    int i;
    bee_http_header_t *header = NULL;

    if (http_request->url != NULL) {
        free(http_request->url);
        http_request->url = NULL;
    }

    if (http_request->method != NULL) {
        free(http_request->method);
        http_request->method = NULL;
    }

    for (i = 0; i < http_request->header_lines; ++i) {
        header = &http_request->headers[i];
        if (header->field != NULL) {
            free(header->field);
            header->field = NULL;
        }
        if (header->value != NULL) {
            free(header->value);
            header->value = NULL;
        }
    }

    http_request->header_lines = 0;

    if (http_request->body != NULL) {
        free(http_request->body);
        http_request->body = NULL;
        http_request->body_length = 0;
    }

    free(http_request);
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
    bee_http_request_t *http_request = parser->data;
    const char *method = http_method_str((enum http_method)parser->method);

    len = strlen(method);
    http_request->method = malloc(len + 1);
    assert(http_request->method != NULL);
    strncpy(http_request->method, method, len);
    http_request->method[len] = '\0';

    return 0;
}

static int
__on_url(http_parser *parser, const char *at, size_t len)
{
    bee_http_request_t *http_request = parser->data;

    http_request->url = malloc(len + 1);
    assert(http_request->url != NULL);
    strncpy(http_request->url, at, len);
    http_request->url[len] = '\0';
    return 0;
}

static int
__on_header_field(http_parser *parser, const char *at, size_t len)
{
    bee_http_request_t *http_request = parser->data;
    bee_http_header_t *header = &http_request->headers[http_request->header_lines];

    header->field = malloc(len + 1);
    assert(header->field != NULL);
    header->field_length = len;
    strncpy(header->field, at, len);
    header->field[len] = '\0';
    return 0;
}

static int
__on_header_value(http_parser *parser, const char *at, size_t len)
{
    bee_http_request_t *http_request = parser->data;
    bee_http_header_t *header = &http_request->headers[http_request->header_lines];

    header->value = malloc(len + 1);
    assert(header->value != NULL);
    header->value_length = len;
    strncpy(header->value, at, len);
    header->value[len] = '\0';
    ++http_request->header_lines;

    return 0;
}

static int
__on_body(http_parser *parser, const char *at, size_t len)
{
    bee_http_request_t *http_request = parser->data;

    http_request->body = malloc(len + 1);
    assert(http_request->body != NULL);
    http_request->body_length = len;
    strncpy(http_request->body, at, len);
    http_request->body[len] = '\0';

    return 0;
}

static int
__on_message_complete(http_parser *parser)
{
    bee_http_request_t *http_request = parser->data;
    int i;

    bee_http_header_t *header = NULL;

    printf("url: %s\n", http_request->url);
    printf("method: %s\n", http_request->method);
    for (i = 0; i < MAX_HTTP_HEADERS; ++i) {
        header = &http_request->headers[i];
        if (header->field) {
            printf("header: %s: %s\n", header->field, header->value);
        }
    }
    printf("body: %s\n", http_request->body);
    printf("\n\n");

    return 0;
}
/*---------------------------------------------------------------------------*/




/*---------------------------------------------------------------------------*/
/* Bee server callbacks                                                      */
/*---------------------------------------------------------------------------*/
enum BEE_HOOK_RESULT httpd_recv(int sfd, void *arg)
{
    bee_connection_t *conn = arg;
    bee_http_t *http = conn->server->pdata;
    bee_http_request_t *http_request;
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

    http_request = __http_request_new();
    if (http_request == NULL)
        return BEE_HOOK_ERR;

    http_parser_init(&parser, HTTP_REQUEST);
    parser.data = http_request;
    nparsed = http_parser_execute(&parser, &http->parser_settings, buf, nr);
    if (nparsed < nr)
        fprintf(stderr, "parse error.\n");
    else {
        /* handle the http request */
        bee_http_callback_t *callback;
        int found = 0;

        TAILQ_FOREACH(callback, &http->callbacks, next) {
            if (strcmp(callback->path, http_request->url) == 0) {
                callback->cb(sfd, http_request);
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

    __http_request_free(http_request);
    return BEE_HOOK_CLOSED;
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Exported functions                                                        */
/*---------------------------------------------------------------------------*/
bee_server_t *
bee_httpd_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog)
{
    bee_http_t * http;
    bee_server_t * server;

    http = calloc(1, sizeof(*http));
    if (!http)
        return NULL;

    http->parser_settings.on_message_begin = __on_message_begin;
    http->parser_settings.on_url = __on_url;
    http->parser_settings.on_header_field = __on_header_field;
    http->parser_settings.on_header_value = __on_header_value;
    http->parser_settings.on_headers_complete = __on_headers_complete;
    http->parser_settings.on_body = __on_body;
    http->parser_settings.on_message_complete = __on_message_complete;
    TAILQ_INIT(&http->callbacks);

    server = bee_server_tcp_new(evbase, baddr, port, backlog);
    if (!server) {
        free(http);
        return NULL;
    }

    server->pdata = http;
    server->on_recv = httpd_recv;

    return server;
}

void
bee_httpd_free(bee_server_t *server)
{
    bee_http_t * http = server->pdata;
    bee_http_callback_t *callback, *tmp;

    TAILQ_FOREACH_SAFE(callback, &http->callbacks, next, tmp) {
        if (callback->path != NULL)
            free(callback->path);

        TAILQ_REMOVE(&http->callbacks, callback, next);
        free(callback);
    }

    free(http);
    bee_server_free(server);
}


bee_http_callback_t *
bee_httpd_set_cb(bee_server_t *server, const char *path, bee_http_callback_cb cb)
{
    bee_http_t * http = server->pdata;
    bee_http_callback_t *callback;

    callback = calloc(1, sizeof(*callback));
    if (!callback)
        return NULL;

    callback->path = strdup(path);
    callback->cb = cb;
    TAILQ_INSERT_TAIL(&http->callbacks, callback, next);

    return callback;
}

