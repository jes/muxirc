/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>

#include "message.h"
#include "server.h"
#include "client.h"
#include "channel.h"

typedef int(*ClientMessageHandler)(Client *, const Message *);

static ClientMessageHandler message_handler[NCOMMANDS];

static int handle_ignore(Client *, const Message *);
static int handle_join(Client *, const Message *);
static int handle_nick(Client *, const Message *);
static int handle_user(Client *, const Message *);
static int handle_privmsg(Client *, const Message *);
static int handle_quit(Client *, const Message *);

/* initialise handler functions for client messages */
void init_client_handlers(void) {
    message_handler[CMD_JOIN] = handle_join;
    message_handler[CMD_NICK] = handle_nick;
    message_handler[CMD_USER] = handle_user;
    message_handler[CMD_PRIVMSG] = handle_privmsg;
    message_handler[CMD_QUIT] = handle_quit;
    message_handler[CMD_CAP] = handle_ignore;
}

/* return a new empty client */
Client *new_client(void) {
    Client *c = malloc(sizeof(Client));
    memset(c, 0, sizeof(Client));
    c->fd = -1;
    return c;
}

/* remove c from its list and free it */
void free_client(Client *c) {
    if(c->prev)
        c->prev->next = c->next;
    if(c->next)
        c->next->prev = c->prev;

    free(c);
}

/* prepend a client to the list pointed to by list, point list to c,
 * and return the new head
 */
Client *prepend_client(Client *c, Client **list) {
    if(*list) {
        if((*list)->prev)
            (*list)->prev->next = c;
        c->prev = (*list)->prev;
        c->next = *list;
        (*list)->prev = c;
    } else {
        c->prev = NULL;
        c->next = NULL;
    }

    *list = c;

    return *list;
}

/* disconnect, remove and free this client */
void disconnect_client(Client *c) {
    /* TODO: remove any channels he is the sole member of */

    close(c->fd);

    /* if this is the first client in the list, point the list at the next
     * client
     */
    if(c->server->client_list == c)
        c->server->client_list = c->next;

    free_client(c);
}

/* send the given string to the given client; if len >= 0 it should contain
 * the length of str, otherwise strlen(str) is used to obtain the length
 */
int send_client_string(Client *c, const char *str, ssize_t len) {
    return c->error = send_string(c->fd, str, len);
}

/* send the given message to the given client, and update the client error
 * state
 */
int send_client_message(Client *c, const Message *m) {
    return c->error = send_message(c->fd, m);
}

/* send a message to the given client, in the form:
 *  :nick!user@host <command> <params...>
 */
int send_client_messagev(Client *c, const char *nick, const char *user,
        const char *host, int command, ...) {
    va_list argp;
    Message *m = new_message();

    if(nick)
        m->nick = strdup(nick);
    if(user)
        m->user = strdup(user);
    if(host)
        m->host = strdup(host);
    m->command = command;

    va_start(argp, command);

    char *s;
    while(1) {
        s = va_arg(argp, char *);
        if(!s)
            break;

        add_message_param(m, strdup(s));
    }

    va_end(argp);

    int r = send_client_message(c, m);
    free_message(m);

    return r;
}

/* read from the client and deal with the messages */
void handle_client_data(Client *c) {
    printf("Handling client data\n");

    /* read data into the buffer and handle messages if there is no error */
    if((c->error = read_data(c->fd, c->buf, &(c->bytes), 1024)) == 0)
        handle_messages(c->buf, &(c->bytes),
                (GenericMessageHandler)handle_client_message, c);
}

/* handle a message from the given client (ignore any invalid ones) */
int handle_client_message(Client *c, const Message *m) {
    if(m->command >= 0 && m->command < NCOMMANDS
            && message_handler[m->command]) {
        return message_handler[m->command](c, m);
    } else {
        /* pass on un-handled messages */
        send_server_message(c->server, m);
        return 0;
    }
}

/* ignore this message */
static int handle_ignore(Client *c, const Message *m) {
    return 0;
}

/* join the channel */
static int handle_join(Client *c, const Message *m) {
    if(m->nparams < 1)
        return send_client_messagev(c, c->server->host, NULL, NULL,
                ERR_NEEDMOREPARAMS, c->server->nick, "JOIN",
                "Not enough parameters", NULL);

    return client_join_channel(c, m->param[0]);
}

/* change our nick unless this is the first NICK sent by this client, in which
 * case change his nick
 */
static int handle_nick(Client *c, const Message *m) {
    if(m->nparams < 1)
        return send_client_messagev(c, c->server->host, NULL, NULL,
                ERR_NEEDMOREPARAMS, c->server->nick, "NICK",
                "Not enough parameters", NULL);

    if(c->gotnick) {
        /* this is not the first nick supplied by this client: the user really
         * wants to change nick, so let's make it happen
         */
        send_server_message(c->server, m);
        return 0;
    } else {
        /* inform the client about what his nick really is */
        c->gotnick = 1;

        return send_client_messagev(c, m->param[0], NULL, NULL, CMD_NICK,
                c->server->nick, NULL);
    }
}

/* tell the client the welcome messages */
static int handle_user(Client *c, const Message *m) {
    int i;

    for(i = 0; i < c->server->nwelcomes; i++)
        if(send_client_string(c, c->server->welcome_line[i], -1))
            break;

    return i != c->server->nwelcomes;
}

/* handle private messages by forwarding them to the server and to other
 * clients that are in the same channel
 */
static int handle_privmsg(Client *c, const Message *m) {
    if(m->nparams < 2)
        return send_client_messagev(c, c->server->host, NULL, NULL,
                ERR_NEEDMOREPARAMS, c->server->nick, "PRIVMSG",
                "Not enough parameters", NULL);

    /* attempt to find a channel with this name */
    Channel *chan = lookup_channel(c->server->channel_list, m->param[0]);

    /* if we are in a channel of this name, tell the other clients in that
     * channel
     */
    if(chan)
        send_channel_messagev(chan, c, c->server->nick, c->server->user,
                c->server->host, m->command, m->param[0], m->param[1], NULL);

    /* always forward the message to the server */
    send_server_message(c->server, m);

    return 0;
}

/* disconnect this client (actually just put it in the error state so that it
 * is disconnected at the next opportunity)
 */
static int handle_quit(Client *c, const Message *m) {
    c->error = 1;
    return 0;
}
