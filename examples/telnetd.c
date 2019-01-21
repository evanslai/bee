#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <errno.h>
#include "bee.h"

/* The maximum length of a telnet line. */
#define TELNETD_LINELEN 36

/* A telnet connection structure. */
struct telnetd_state {
    int sfd;
    char buf[TELNETD_LINELEN];
    int bufptr;
    int state;
};

#define ISO_nl       0x0a
#define ISO_cr       0x0d

#define STATE_NORMAL 0
#define STATE_IAC    1
#define STATE_WILL   2
#define STATE_WONT   3
#define STATE_DO     4
#define STATE_DONT   5
#define STATE_CLOSE  6


#define TELNET_IAC   255
#define TELNET_WILL  251
#define TELNET_WONT  252
#define TELNET_DO    253
#define TELNET_DONT  254



static ssize_t _write(int fd, const void *buf, size_t count)
{
    size_t written = 0;
    ssize_t thisTime = 0;
    while (count != written) {
        thisTime = write(fd, (char *)buf + written, count - written);
        if (thisTime == -1) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN)
                continue;
            else
                return -1;
        }
        written += thisTime;
    }
    return written;
}


/* Open a telnet seesion. */
static struct telnetd_state * telnetd_new(int sfd)
{
    struct telnetd_state *s;

    s = calloc(1, sizeof(*s));
    assert(s != NULL);

    s->sfd = sfd;
    s->bufptr = 0;
    s->state = STATE_NORMAL;

    return s;
}

/* Close a telnet session.
 * 
 * This function can be called from a telnet command in order to close
 * the connection.
 */
static void telnetd_free(struct telnetd_state *s)
{
    free(s);
}

/* Print a prompt on a telnet connection.
 *
 * This function can be called by the telnet command shell in order to
 * print out a command prompt.
 */
static void telnetd_prompt(struct telnetd_state *s, char *str)
{
    ssize_t nr;
    char *linebuf;

    linebuf = calloc(sizeof(char), TELNETD_LINELEN);
    assert(linebuf != NULL);
    strncpy(linebuf, str, TELNETD_LINELEN);
    nr = _write(s->sfd, linebuf, TELNETD_LINELEN);
    if (nr < 0)
        perror("write");

    free(linebuf);
}

static void telnetd_println(struct telnetd_state *s, char *str)
{
    ssize_t nr;
    char *linebuf;
    int len;

    linebuf = calloc(sizeof(char), TELNETD_LINELEN);
    assert(linebuf != NULL);
    strncpy(linebuf, str, TELNETD_LINELEN);
    len = strlen(linebuf);
    if (len < TELNETD_LINELEN - 2) {
        linebuf[len] = ISO_cr;
        linebuf[len+1] = ISO_nl;
        linebuf[len+2] = 0;
    }
    nr = _write(s->sfd, linebuf, TELNETD_LINELEN);
    if (nr < 0)
        perror("write");

    free(linebuf);
}




static void
__getchar(struct telnetd_state *s, uint8_t c)
{
    static const char *ctrl_c =
        "\xFF\xF4\xFF\xFD\x06";

    if (c == ISO_cr) {
        return;
    }

    s->buf[s->bufptr] = c;
    if ((s->buf[s->bufptr] == ISO_nl) ||
        (s->bufptr == sizeof(s->buf) - 1))
    {
        if (s->bufptr > 0) {
            s->buf[s->bufptr] = 0;
        }

        if (strcmp("quit", s->buf) == 0) {
            s->state = STATE_CLOSE;
        }
        else {
            telnetd_println(s, s->buf);
        }
        telnetd_prompt(s, "wizard> ");
        s->bufptr = 0;
    }
    else
        ++s->bufptr;
}

static void
__sendopt(struct telnetd_state *s, uint8_t option, uint8_t value)
{
    ssize_t nr;
    char *linebuf;

    linebuf = calloc(sizeof(char), TELNETD_LINELEN);
    assert(linebuf != NULL);

    linebuf[0] = TELNET_IAC;
    linebuf[1] = option;
    linebuf[2] = value;
    linebuf[3] = 0;
    nr = _write(s->sfd, linebuf, TELNETD_LINELEN);
    if (nr < 0)
        perror("write");

    free(linebuf);
}



/*-----------------------------------------------------------------------------------*/

enum BEE_HOOK_RESULT telnetd_recv(int sfd, void *arg)
{
    bee_connection_t * conn = arg;
    struct telnetd_state *s = conn->pdata;
    unsigned char c;
    ssize_t nr;

    nr = recv(sfd, &c, 1, 0);
    if (nr < 0) {
        if (errno == EAGAIN)
            return BEE_HOOK_EAGAIN;
        perror("recv");
        telnetd_free(s);
        return BEE_HOOK_ERR;
    }
    else if (nr == 0) {
        telnetd_free(s);
        return BEE_HOOK_PEER_CLOSED;
    }

    //printf("recv: %02x\n", c);
    if (nr > 0 && (s->bufptr < sizeof(s->buf))) {
        switch (s->state) {
        case STATE_IAC:
            if (c == TELNET_IAC) {
                __getchar(s, c);
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
            __sendopt(s, TELNET_DONT, c);
            s->state = STATE_NORMAL;
            break;
        case STATE_WONT:
            /* Reply with a DONT */
            __sendopt(s, TELNET_DONT, c);
            s->state = STATE_NORMAL;
            break;
        case STATE_DO:
            /* Reply with a WONT */
            __sendopt(s, TELNET_WONT, c);
            s->state = STATE_NORMAL;
            break;
        case STATE_DONT:
            /* Reply with a WONT */
            __sendopt(s, TELNET_WONT, c);
            s->state = STATE_NORMAL;
            break;
        case STATE_NORMAL:
            if (c == TELNET_IAC) {
                s->state = STATE_IAC;
            }
            else {
                __getchar(s, c);
            }
        }
    }

    if (s->state == STATE_CLOSE) {
        telnetd_println(s, "goodbye");
        telnetd_free(s);
        return BEE_HOOK_PEER_CLOSED;
    }


    return BEE_HOOK_OK;
}

enum BEE_HOOK_RESULT telnetd_accept(int sfd, void *arg)
{
    bee_connection_t *conn = arg;
    struct telnetd_state *s = telnetd_new(sfd);
    conn->pdata = s;
    telnetd_prompt(s, "bee> ");
    return BEE_HOOK_OK;
}

int main(int argc, char **argv)
{
    struct event_base *evbase = event_base_new();
    bee_server_t *server = bee_server_tcp_new(evbase, "0.0.0.0", 8000, -1);

    server->on_accept = telnetd_accept;
    server->on_recv = telnetd_recv;

    printf("Start telnet CLI server with port 8000\n");
    event_base_loop(evbase, 0);
    bee_server_free(server);
    event_base_free(evbase);

    return 0;
}

