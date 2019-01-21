#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "bee.h"
#include "http_parser.h"

#define MAX_HTTP_HEADERS        (128)

#define RESPONSE                    \
    "HTTP/1.1 200 OK\r\n"           \
    "Content-Type: text/plain\r\n"  \
    "Content-Length: 14\r\n"        \
    "\r\n"                          \
    "Hello, World!\n"

/* Represents a single http header. */
typedef struct {
    char *field;
    char *value;
    size_t field_length;
    size_t value_length;
} http_header_t;

/* Represents a http request with internal dependencies. */
typedef struct {
    char *url;
    char *method;
    int header_lines;
    http_header_t headers[MAX_HTTP_HEADERS];
    char *body;
    size_t body_length;
} http_request_t;

static http_parser_settings parser_settings;


http_request_t * http_request_new(void)
{
    http_request_t *http_request;
    http_header_t *header = NULL;
    int i;

    http_request = malloc(sizeof(http_request_t));
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

void http_request_free(http_request_t *http_request)
{
    int i;
    http_header_t *header = NULL;

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


/* http_parser callback: Initialize default values. */
int on_message_begin(http_parser *parser)
{
    return 0;
}

/* http_parser callback: extract the method name. */
int on_headers_complete(http_parser *parser)
{
    size_t len;
    http_request_t *http_request = parser->data;
    const char *method = http_method_str((enum http_method)parser->method);

    len = strlen(method);
    http_request->method = malloc(len + 1);
    assert(http_request->method != NULL);
    strncpy(http_request->method, method, len);
    http_request->method[len] = '\0';

    return 0;
}

/* http_parser callback: copy url string to http_request->url. */
int on_url(http_parser *parser, const char *at, size_t len)
{
    http_request_t *http_request = parser->data;

    http_request->url = malloc(len + 1);
    strncpy(http_request->url, at, len);
    http_request->url[len] = '\0';
    return 0;
}

/* http_parser callback: copy the header field name to the current header item. */
int on_header_field(http_parser *parser, const char *at, size_t len)
{
    http_request_t *http_request = parser->data;
    http_header_t *header = &http_request->headers[http_request->header_lines];

    header->field = malloc(len + 1);
    header->field_length = len;
    strncpy(header->field, at, len);
    header->field[len] = '\0';
    return 0;
}

/* http_parser callback: copy its assigned value. */
int on_header_value(http_parser *parser, const char *at, size_t len)
{
    http_request_t *http_request = parser->data;
    http_header_t *header = &http_request->headers[http_request->header_lines];

    header->value = malloc(len + 1);
    header->value_length = len;
    strncpy(header->value, at, len);
    header->value[len] = '\0';
    ++http_request->header_lines;

    return 0;
}

/* http_parser callback: on_body */
int on_body(http_parser *parser, const char *at, size_t len)
{
    http_request_t *http_request = parser->data;

    http_request->body = malloc(len + 1);
    http_request->body_length = len;
    strncpy(http_request->body, at, len);
    http_request->body[len] = '\0';

    return 0;
}

/* http_parser callback: on_message_complete */
int on_message_complete(http_parser *parser)
{
    http_request_t *http_request = parser->data;
    int i;

    http_header_t *header = NULL;

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


enum BEE_HOOK_RESULT httpd_recv(int sfd, void *arg)
{
    bee_connection_t *conn = arg;
    http_parser parser;
    http_request_t *http_request;
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

    http_request = http_request_new();
    http_parser_init(&parser, HTTP_REQUEST);
    parser.data = http_request;
    nparsed = http_parser_execute(&parser, &parser_settings, buf, nr);
    if (nparsed < nr)
        fprintf(stderr, "parse error.\n");
    else {
        nr = send(sfd, RESPONSE, sizeof(RESPONSE), 0);
        if (nr < 0) {
            perror("send");
            return BEE_HOOK_ERR;
        }
    }

    http_request_free(http_request);
    return BEE_HOOK_OK;
}


int main(int argc, char **argv)
{
    
    struct event_base *evbase = event_base_new();
    bee_server_t *server = bee_server_tcp_new(evbase, "0.0.0.0", 8000, -1);

    parser_settings.on_message_begin = on_message_begin;
    parser_settings.on_url = on_url;
    parser_settings.on_header_field = on_header_field;
    parser_settings.on_header_value = on_header_value;
    parser_settings.on_headers_complete = on_headers_complete;
    parser_settings.on_body = on_body;
    parser_settings.on_message_complete = on_message_complete;

    server->on_recv = httpd_recv;
    printf("Start http server with port 8000\n");
    event_base_loop(evbase, 0);
    bee_server_free(server);
    event_base_free(evbase);

    return 0;
}
