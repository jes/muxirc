/* Server handling for muxirc
 *
 * James Stanley 2012
 */

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include "message.h"
#include "server.h"
#include "client.h"
#include "channel.h"

typedef int(*ServerMessageHandler)(Server *, const Message *);

static ServerMessageHandler message_handler[NCOMMANDS];

static int handle_join(Server *, const Message *);

/* initialise handler functions for server messages */
void init_server_handlers(void) {
    message_handler[CMD_JOIN] = handle_join;
}

/* connect to irc and initialise the server state */
void irc_connect(Server *s, const char *server, const char *port,
        const char *username, const char *realname, const char *nick) {
    struct addrinfo hints, *servinfo, *p;
    int n;
    int fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((n = getaddrinfo(server, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(n));
        exit(1);
    }

    /* find a working connection */
    for(p = servinfo; p; p = p->ai_next) {
        if((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if(connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            perror("connect");
            continue;
        }

        /* successfully socket'd and connect'd, use this fd */
        break;
    }

    if(!p) {
        fprintf(stderr, "error: failed to connect to %s:%s\n",
                server, port);
        exit(1);
    }

    freeaddrinfo(servinfo);

    s->fd = fd;
    s->error = 0;
    s->nick = nick;
    s->user = username;
    s->host = NULL;
    s->channel_list = NULL;
    s->client_list = NULL;

    Message *m = new_message();
    m->param = malloc(4 * sizeof(char *));

    /* TODO: Send PASS if necessary */
    m->command = CMD_NICK;
    m->nparams = 1;
    m->param[0] = strdup(nick);
    send_message(fd, m);
    free(m->param[0]);

    m->command = CMD_USER;
    m->nparams = 4;
    m->param[0] = strdup(username);
    m->param[1] = strdup("hostname");/* TODO: this */
    m->param[2] = strdup(server);
    m->param[3] = strdup(realname);
    send_message(fd, m);

    free_message(m);
}

/* send the given message to all clients */
int send_all_clients(Server *s, const Message *m) {
    Client *c;
    size_t msglen;
    char *strmsg = strmessage(m, &msglen);

    for(c = s->client_list; c; c = c->next)
        send_client_string(c, strmsg, msglen);

    free(strmsg);

    return 0;
}

/* send the given string to the given server; if len >= 0 it should contain
 * the length of str, otherwise strlen(str) is used to obtain the length
 */
int send_server_string(Server *s, const char *str, ssize_t len) {
    return s->error = send_string(s->fd, str, len);
}

/* send the given message to the given server, and update the server error
 * state
 */
int send_server_message(Server *s, const Message *m) {
    return s->error = send_message(s->fd, m);
}

/* send a message to the given server, in the form:
 *  <command> <params...>
 */
int send_server_messagev(Server *s, int command, ...) {
    va_list argp;
    Message *m = new_message();

    m->nick = strdup("muxirc");
    m->command = command;

    va_start(argp, command);

    char *str;
    while(1) {
        str = va_arg(argp, char *);
        if(!str)
            break;

        add_message_param(m, strdup(str));
    }

    va_end(argp);

    int r = send_server_message(s, m);
    free_message(m);

    return r;
}


/* handle a message from the server (ignore any invalid ones) */
int handle_server_message(Server *s, const Message *m) {
    /* set the user and host of the server state if it doesn't already
     * have one and the message does
     */
    if(!s->user || !s->host) {
        if(strcasecmp(m->nick, s->nick) == 0) {
            if(m->user)
                s->user = m->user;
            if(m->host)
                s->host = m->host;
        }
    }

    /* call the handler if there is one, otherwise just ignore the message */
    if(m->command >= 0 && m->command < NCOMMANDS
            && message_handler[m->command])
        return message_handler[m->command](s, m);
    else
        return 0;
}

/* handle a join message by telling all clients about the join, and joining it
 * if the joiner is us; return 0 on success and -1 on error
 */
int handle_join(Server *s, const Message *m) {
    /* fail if there are too few parameters or there is no nick */
    if(!m->nick || m->nparams == 0)
        return -1;

    if(strcasecmp(m->nick, s->nick) == 0) {
        joined_channel(s, m->param[0], m);
    } else {
        Channel *chan = lookup_channel(s->channel_list, m->param[0]);
        send_channel_message(chan, m);
    }

    return 0;
}
