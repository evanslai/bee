#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include "bee.h"
#include "bee_telnet.h"

#define ISO_nl       0x0a
#define ISO_cr       0x0d

#define STATE_NORMAL 0
#define STATE_IAC    1
#define STATE_WILL   2
#define STATE_WONT   3
#define STATE_DO     4
#define STATE_DONT   5
#define STATE_CLOSE  6
#define STATE_READY 7

#define TELNET_IAC   255
#define TELNET_WILL  251
#define TELNET_WONT  252
#define TELNET_DO    253
#define TELNET_DONT  254


#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)          \
    for ((var) = TAILQ_FIRST((head));                       \
         (var) && ((tvar) = TAILQ_NEXT((var), field), 1);   \
         (var) = (tvar))
#endif


#define TELNET_LINELEN 80

typedef struct {
    int     sfd;
    char    buf[TELNET_LINELEN];
    int     bufptr;
    int     state;
} telnet_state_t;


static telnet_state_t *
telnet_state_new(int sfd)
{
    telnet_state_t *s;

    s = calloc(1, sizeof(*s));
    assert(s != NULL);

    s->sfd = sfd;
    s->bufptr = 0;
    s->state = STATE_NORMAL;

    return s;
}


static void
telnet_state_free(telnet_state_t *s)
{
    free(s);
}

static int
setargs(char *args, char **argv)
{
    int count = 0;

    while (isspace(*args))
        ++args;
    while (*args) {
        if (argv)
            argv[count] = args;
        while (*args && !isspace(*args))
            ++args;
        if (argv && *args)
            *args++ = '\0';
        while (isspace(*args))
            ++args;
        count++;
    }
    return count;
}

static char **
parsedargs(char *args, int *argc)
{
    char **argv = NULL;
    int    argn = 0;

    if (args && *args
        && (args = strdup(args))
        && (argn = setargs(args,NULL))
        && (argv = malloc((argn+1) * sizeof(char *))))
    {
        *argv++ = args;
        argn = setargs(args,argv);
    }

    if (args && !argv)
        free(args);

    *argc = argn;
    return argv;
}

void
freeparsedargs(char **argv)
{
    if (argv) {
        free(argv[-1]);
        free(argv-1);
    } 
}

static void
get_char(telnet_state_t *s, uint8_t c)
{
    if (c == ISO_cr)
        return;

    s->buf[s->bufptr] = c;
    if ((s->buf[s->bufptr] == ISO_nl) ||
        (s->bufptr == sizeof(s->buf) - 1))
    {
        if (s->bufptr > 0) {
            s->buf[s->bufptr] = 0;
        }

        s->bufptr = 0;
    }
    else
        ++s->bufptr;
}


static void
sendopt(telnet_state_t *s, uint8_t option, uint8_t value)
{
    ssize_t nr;
    char *linebuf;
 
    linebuf = calloc(sizeof(char), TELNET_LINELEN);
    assert(linebuf != NULL);
 
    linebuf[0] = TELNET_IAC;
    linebuf[1] = option;
    linebuf[2] = value;
    linebuf[3] = 0;
    nr = send(s->sfd, linebuf, TELNET_LINELEN, 0);
    if (nr < 0)
        perror("send");

    free(linebuf);
}



/*---------------------------------------------------------------------------*/
/* Bee server callbacks                                                      */
/*---------------------------------------------------------------------------*/
enum BEE_HOOK_RESULT telnetd_recv(int sfd, void *arg)
{
    bee_connection_t * conn = arg;
    bee_telnet_t *telnet = conn->server->pdata;
    telnet_state_t *s = conn->pdata;
    unsigned char c;
    ssize_t nr;

    nr = recv(sfd, &c, 1, 0);
    if (nr < 0) {
        if (errno == EAGAIN)
            return BEE_HOOK_EAGAIN;
        perror("recv");
        telnet_state_free(s);
        return BEE_HOOK_ERR;
    }
    else if (nr == 0) {
        telnet_state_free(s);
        return BEE_HOOK_PEER_CLOSED;
    }

    if (nr > 0 && (s->bufptr < sizeof(s->buf))) {
        switch (s->state) {
        case STATE_IAC:
            if (c == TELNET_IAC) {
                get_char(s, c);
                s->state = STATE_NORMAL;
            }
            else {
                switch(c) {
                case TELNET_WILL:
                    s->state = STATE_WILL;
                    break;
                case TELNET_WONT:
                    s->state = STATE_WONT;
                    break;
                case TELNET_DO:
                    s->state = STATE_DO;
                    break;
                case TELNET_DONT:
                    s->state = STATE_DONT;
                    break;
                default:
                    s->state = STATE_NORMAL;
                    break;
                }
            }
            break;
        case STATE_WILL:
            /* Reply with a DONT */
            sendopt(s, TELNET_DONT, c);
            s->state = STATE_NORMAL;
            break;
        case STATE_WONT:
            /* Reply with a DONT */
            sendopt(s, TELNET_DONT, c);
            s->state = STATE_NORMAL;
            break;
        case STATE_DO:
            /* Reply with a WONT */
            sendopt(s, TELNET_WONT, c);
            s->state = STATE_NORMAL;
            break;
        case STATE_DONT:
            /* Reply with a WONT */
            sendopt(s, TELNET_WONT, c);
            s->state = STATE_NORMAL;
            break;
        case STATE_NORMAL:
            if (c == TELNET_IAC) {
                s->state = STATE_IAC;
            }
            else {
                get_char(s, c);
            }
        }
    }

    if (s->bufptr == 0) {
        bee_telnet_callback_t *callback;
        int found = 0;

        if (strcmp(s->buf, "quit") == 0) {
            bee_telnetd_println(sfd, "goodbye");
            telnet_state_free(s);
            return BEE_HOOK_PEER_CLOSED;
        }

        TAILQ_FOREACH(callback, &telnet->callbacks, next) {
            if (strncmp(callback->path, s->buf, strlen(callback->path)) == 0) {
                int argc;
                char **argv;

                argv = parsedargs(s->buf, &argc);
                callback->cb(sfd, argc, argv);
                freeparsedargs(argv);
                found = 1;
                break;
            }
        }

        if (!found)
            bee_telnetd_println(sfd, "command not found");
        bee_telnetd_prompt(sfd, "bee> ");
    }

    return BEE_HOOK_OK;
}

enum BEE_HOOK_RESULT telnetd_accept(int sfd, void *arg)
{
    bee_connection_t *conn = arg;
    telnet_state_t *s = telnet_state_new(sfd);
    conn->pdata = s;
    bee_telnetd_prompt(sfd, "bee> ");
    return BEE_HOOK_OK;
}


/*---------------------------------------------------------------------------*/
/* Exported functions                                                        */
/*---------------------------------------------------------------------------*/
bee_server_t *
bee_telnetd_new(struct event_base *evbase, const char *baddr, uint16_t port, int backlog)
{
    bee_telnet_t * telnet;
    bee_server_t * server;

    telnet = calloc(1, sizeof(*telnet));
    if (!telnet)
        return NULL;

    TAILQ_INIT(&telnet->callbacks);

    server = bee_server_tcp_new(evbase, baddr, port, backlog);
    if (!server) {
        free(telnet);
        return NULL;
    }

    server->pdata = telnet;
    server->on_accept = telnetd_accept;
    server->on_recv = telnetd_recv;

    return server;
}

void
bee_telnetd_free(bee_server_t *server)
{
    bee_telnet_t *telnet = server->pdata;
    bee_telnet_callback_t *callback, *tmp;

    TAILQ_FOREACH_SAFE(callback, &telnet->callbacks, next, tmp) {
        if (callback->path != NULL)
            free(callback->path);

        TAILQ_REMOVE(&telnet->callbacks, callback, next);
        free(callback);
    }

    free(telnet);
    bee_server_free(server);
}

bee_telnet_callback_t *
bee_telnetd_set_cb(bee_server_t *server, const char *path, bee_telnet_callback_cb cb)
{
    bee_telnet_t * telnet = server->pdata;
    bee_telnet_callback_t *callback;

    callback = calloc(1, sizeof(*callback));
    if (!callback)
        return NULL;

    callback->path = strdup(path);
    callback->cb = cb;
    TAILQ_INSERT_TAIL(&telnet->callbacks, callback, next);

    return callback;
}

void
bee_telnetd_prompt(int sfd, char *str)
{
    ssize_t nr;
    char *linebuf;
 
    linebuf = calloc(sizeof(char), TELNET_LINELEN);
    assert(linebuf != NULL);
    strncpy(linebuf, str, TELNET_LINELEN);
    nr = send(sfd, linebuf, TELNET_LINELEN, 0);
    if (nr < 0)
        perror("write");
 
    free(linebuf);
}

void
bee_telnetd_println(int sfd, const char *fmt, ...)
{
    ssize_t nr;
    char *linebuf;
    int len;
    va_list arg;
 
    linebuf = calloc(sizeof(char), TELNET_LINELEN);
    assert(linebuf != NULL);

    va_start(arg, fmt);
    vsnprintf(linebuf, TELNET_LINELEN, fmt, arg);
    va_end(arg);

    len = strlen(linebuf);
    if (len < TELNET_LINELEN - 2) {
        linebuf[len] = ISO_cr;
        linebuf[len+1] = ISO_nl;
        linebuf[len+2] = 0;
    }
    nr = send(sfd, linebuf, TELNET_LINELEN, 0);
    if (nr < 0)
        perror("send");
 
    free(linebuf);
}
