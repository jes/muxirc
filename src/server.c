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
#include <errno.h>

#include "socket.h"
#include "message.h"
#include "server.h"
#include "client.h"
#include "channel.h"

typedef int(*ServerMessageHandler)(Server *, const Message *);

static ServerMessageHandler message_handler[NCOMMANDS];

static int handle_ignore(Server *, const Message *);
static int handle_join(Server *, const Message *);
static int handle_nick(Server *, const Message *);
static int handle_topic(Server *, const Message *);
static int handle_welcome(Server *, const Message *);
static int handle_motd(Server *, const Message *);

/* initialise handler functions for server messages */
void init_server_handlers(void) {
    message_handler[CMD_JOIN] = handle_join;
    message_handler[CMD_NICK] = handle_nick;
    message_handler[CMD_TOPIC] = handle_topic;
    message_handler[RPL_TOPIC] = handle_topic;
    message_handler[CMD_CAP] = handle_ignore;
    message_handler[RPL_WELCOME] = handle_welcome;
    message_handler[RPL_YOURHOST] = handle_welcome;
    message_handler[RPL_CREATED] = handle_welcome;
    message_handler[RPL_MYINFO] = handle_welcome;
    message_handler[RPL_ISUPPORT] = handle_welcome;
    message_handler[RPL_MOTDSTART] = handle_motd;
    message_handler[RPL_MOTD] = handle_motd;
    message_handler[RPL_ENDOFMOTD] = handle_motd;
}

/* connect to irc and initialise the server state */
void irc_connect(Server *s, const char *server, const char *serverport,
        const char *username, const char *realname, const char *nick,
        const char *listenport) {
    int yes = 1;
    struct addrinfo hints, *servinfo, *p;
    int n;
    int fd;

    /* initialise all of s */
    memset(s, 0, sizeof(Server));
    s->listenfd = -1;
    s->nick = strdup(nick);
    s->sock = new_socket();

    /* setup hints for the listening socket */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((n = getaddrinfo(NULL, listenport, &hints, &servinfo))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(n));
        close(s->sock->fd);
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

    s->sock->fd = fd;

    /* now register with the server */
    /* TODO: Send PASS if necessary */
    send_socket_messagev(s->sock, NULL, NULL, NULL, CMD_NICK, nick, NULL);
    send_socket_messagev(s->sock, NULL, NULL, NULL, CMD_USER, username,
            "localhost", server, realname, NULL);

    /* TODO: Something that will reliably cause us to be told our user and
     * host so that it will get set (send us a pm?). We need to give the
     * server long enough to decide whether or not our nick is taken though.
     * Perhaps waiting for any of:
     *  1.) A 001 (RPL_WELCOME) message (success)
     *  2.) 5 seconds to pass (assume success)
     *  3.) A 433 (ERR_NICKNAMEINUSE) message (failure)
     */
}

/* send the given message to all clients */
int send_all_clients(Server *s, const Message *m) {
    Client *c;
    size_t msglen;
    char *strmsg = strmessage(m, &msglen);

    for(c = s->client_list; c; c = c->next)
        send_socket_string(c->sock, strmsg, msglen);

    free(strmsg);

    return 0;
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
    c->sock->fd = fd;
    c->server = s;
    prepend_client(c, &(s->client_list));
}

/* handle data from the server by splitting it up and handling any lines that
 * are received
 */
void handle_server_data(Server *s) {
    printf("Handling server data\n");

    /* read data into the buffer and handle messages if there is no error */
    if(read_data(s->sock) == 0)
        handle_messages(s->sock->buf, &(s->sock->bytes),
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
            s->user = strdup(m->user);
        if(m->host)
            s->host = strdup(m->host);
    }

    if(m->command >= FIRST_CMD && m->command < NCOMMANDS)
        fprintf(stderr, "handled server message: %s\n",
                command_string[m->command - FIRST_CMD]);
    else
        fprintf(stderr, "handled server message: %03d\n", m->command);

    /* call the handler if there is one, otherwise just ignore the message */
    if(m->command >= 0 && m->command < NCOMMANDS
            && message_handler[m->command]) {
        return message_handler[m->command](s, m);
    } else {
        printf("not handling....!\n");
        /* pass un-handled messages to all clients */
        send_all_clients(s, m);
        return 0;
    }
}

/* ignore this message */
static int handle_ignore(Server *s, const Message *m) {
    return 0;
}

/* handle a join message by telling all clients about the join, and joining it
 * if the joiner is us; return 0 on success and -1 on error
 */
static int handle_join(Server *s, const Message *m) {
    /* fail if there are too few parameters or there is no nick */
    if(!m->nick || m->nparams == 0)
        return -1;

    if(strcasecmp(m->nick, s->nick) == 0) {
        joined_channel(s, m->param[0], m);
    } else {
        Channel *chan = lookup_channel(s->channel_list, m->param[0]);
        send_channel_message(chan, NULL, m);
    }

    return 0;
}

/* handle a nick message by telling all clients about the nick, and changing
 * our nick if the old one was us
 */
static int handle_nick(Server *s, const Message *m) {
    /* fail if there are too few parameters or there is no nick */
    if(!m->nick || m->nparams == 0)
        return -1;

    /* TODO: if the nick change is not for us, only tell clients in the same
     * channel as the person who changed the nick
     */
    send_all_clients(s, m);

    /* if the nick change is for us, update our nick */
    if(strcasecmp(m->nick, s->nick) == 0) {
        free(s->nick);
        s->nick = strdup(m->param[0]);

        /* also change all of the welcome message nicks */
        /* TODO: it is pretty inefficient to keep around hundreds of copies of
         * the nick...
         */
        int i;
        for(i = 0; i < s->nwelcomes; i++) {
            if(s->welcomemsg[i]->nparams > 0) {
                free(s->welcomemsg[i]->param[0]);
                s->welcomemsg[i]->param[0] = strdup(s->nick);
            }
        }
    }

    return 0;
}

/* handle a topic change */
static int handle_topic(Server *s, const Message *m) {
    printf("m->nparams = %d\n", m->nparams);

    /* not enough parameters: fail */
    if((m->command != CMD_TOPIC && m->nparams < 3) || m->nparams < 2)
        return -1;

    printf("accepted m->nparams\n");

    /* if this is a numeric topic message, there is an extra parameter
     * containing our nick
     */
    char **param = m->param + (m->command != CMD_TOPIC);

    Channel *chan = lookup_channel(s->channel_list, param[0]);

    printf("chan %s = %p\n", param[0], chan);

    /* not in the channel: fail */
    if(!chan)
        return -1;

    /* update the topic */
    free(chan->topic);
    chan->topic = strdup(param[1]);

    printf("sending channel a message\n");

    /* tell all clients in the channel */
    send_channel_message(chan, NULL, m);

    return 0;
}

/* handle a welcome message by appending it to the buffer and sending it to
 * any existing clients
 */
static int handle_welcome(Server *s, const Message *m) {
    s->nwelcomes++;
    s->welcomemsg = realloc(s->welcomemsg, s->nwelcomes * sizeof(Message *));
    s->welcomemsg[s->nwelcomes - 1] = copy_message(m);

    send_all_clients(s, m);

    return 0;
}

/* handle motd-related messages by forwarding them to appropriate clients */
static int handle_motd(Server *s, const Message *m) {
    Client *c;

    for(c = s->client_list; c; c = c->next) {
        if(s->motd_state == MOTD_HAPPY
                || (s->motd_state == MOTD_WANT && c->motd_state == MOTD_WANT)
                || (c->motd_state == MOTD_READING)) {
            /* update the client motd state */
            if(c->motd_state == MOTD_WANT)
                c->motd_state = MOTD_READING;
            if(m->command == RPL_ENDOFMOTD)
                c->motd_state = MOTD_HAPPY;

            /* forward the message */
            send_socket_message(c->sock, m);
        }
    }

    /* update the server motd state */
    if(s->motd_state == MOTD_WANT)
        s->motd_state = MOTD_READING;
    if(m->command == RPL_ENDOFMOTD)
        s->motd_state = MOTD_HAPPY;

    return 0;
}
