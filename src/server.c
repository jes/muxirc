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
#include <errno.h>

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
void irc_connect(Server *s, const char *server, const char *serverport,
        const char *username, const char *realname, const char *nick,
        const char *listenport) {
    int yes = 1;
    struct addrinfo hints, *servinfo, *p;
    int n;
    int fd;

    /* initialise all fields of s */
    s->serverfd = -1;
    s->listenfd = -1;
    s->error = 0;
    s->nick = nick;
    s->user = username;
    s->host = NULL;
    s->bytes = 0;
    s->channel_list = NULL;
    s->client_list = NULL;

    /* setup hints for the listening socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((n = getaddrinfo(NULL, listenport, &hints, &servinfo))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(n));
        close(s->serverfd);
        exit(1);
    }

    /* find somewhere to listen */
    for(p = servinfo; p; p = p->ai_next) {
        if((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        /* complain but don't care if this fails */
        if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
            perror("setsockopt");

        if(bind(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            perror("bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if(!p) {
        fprintf(stderr, "error: failed to bind to port %s\n", listenport);
        exit(1);
    }

    if(listen(fd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    s->listenfd = fd;

    /* now setup hints for the connecting socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((n = getaddrinfo(server, serverport, &hints, &servinfo))) {
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

    freeaddrinfo(servinfo);

    if(!p) {
        fprintf(stderr, "error: failed to connect to %s:%s\n",
                server, serverport);
        exit(1);
    }

    s->serverfd = fd;

    /* now register with the server */
    /* TODO: Send PASS if necessary */
    send_server_messagev(s, CMD_NICK, nick, NULL);
    send_server_messagev(s, CMD_USER, username, "localhost", server, realname,
            NULL);
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
    return s->error = send_string(s->serverfd, str, len);
}

/* send the given message to the given server, and update the server error
 * state
 */
int send_server_message(Server *s, const Message *m) {
    return s->error = send_message(s->serverfd, m);
}

/* send a message to the given server, in the form:
 *  <command> <params...>
 */
int send_server_messagev(Server *s, int command, ...) {
    va_list argp;
    Message *m = new_message();

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

/* handle a new connection on the listening socket */
void handle_new_connection(Server *s) {
    int fd;
    struct sockaddr_storage addr;
    socklen_t addrsize = sizeof(addr);

    /* if the acceptance fails for some reason, Keep Calm and Carry On */
    if((fd = accept(s->listenfd, (struct sockaddr *)&addr, &addrsize)) == -1) {
        perror("accept");
        return;
    }

    printf("Got connection\n");

    /* make a new client and add him to the list */
    Client *c = new_client();
    c->fd = fd;
    c->server = s;
    prepend_client(c, &(s->client_list));
}

/* handle data from the server by splitting it up and handling any lines that
 * are received
 */
void handle_server_data(Server *s) {
    printf("Handling server data\n");

    /* read data into the buffer and handle messages if there is no error */
    if((s->error = read_data(s->serverfd, s->buf, &(s->bytes), 1024)) == 0)
        handle_messages(s->buf, &(s->bytes),
                (GenericMessageHandler)handle_server_message, s);
}

/* handle a message from the server (ignore any invalid ones) */
int handle_server_message(Server *s, const Message *m) {
    /* set the user and host of the server state if it doesn't already
     * have one and the message does
     */
    if((!s->user || !s->host) && m->nick && s->nick
            && strcasecmp(m->nick, s->nick) == 0) {
        if(m->user)
            s->user = m->user;
        if(m->host)
            s->host = m->host;
    }

    if(m->command >= FIRST_CMD && m->command < NCOMMANDS)
        fprintf(stderr, "handled server message: %s\n",
                command_string[m->command - FIRST_CMD]);
    else
        fprintf(stderr, "handled server message: %03d\n", m->command);

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
