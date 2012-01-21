/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "socket.h"
#include "message.h"
#include "server.h"
#include "client.h"
#include "channel.h"

typedef int(*ClientMessageHandler)(Client *, const Message *);

static ClientMessageHandler message_handler[NCOMMANDS];

static int handle_ignore(Client *, const Message *);
static int handle_join(Client *, const Message *);
static int handle_part(Client *, const Message *);
static int handle_nick(Client *, const Message *);
static int handle_user(Client *, const Message *);
static int handle_privmsg(Client *, const Message *);
static int handle_quit(Client *, const Message *);

/* initialise handler functions for client messages */
void init_client_handlers(void) {
    message_handler[CMD_JOIN] = handle_join;
    message_handler[CMD_PART] = handle_part;
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
    c->sock = new_socket();
    return c;
}

/* remove c from its list and free it */
void free_client(Client *c) {
    remove_client_from_channels(c);

    if(c->prev)
        c->prev->next = c->next;
    if(c->next)
        c->next->prev = c->prev;

    free(c->sock);
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
    close(c->sock->fd);

    /* if this is the first client in the list, point the list at the next
     * client
     */
    if(c->server->client_list == c)
        c->server->client_list = c->next;

    free_client(c);
}

/* read from the client and deal with the messages */
void handle_client_data(Client *c) {
    printf("Handling client data\n");

    /* read data into the buffer and handle messages if there is no error */
    if(read_data(c->sock) == 0)
        handle_messages(c->sock->buf, &(c->sock->bytes),
                (GenericMessageHandler)handle_client_message, c);
}

/* handle a message from the given client (ignore any invalid ones) */
int handle_client_message(Client *c, const Message *m) {
    if(m->command >= 0 && m->command < NCOMMANDS
            && message_handler[m->command]) {
        return message_handler[m->command](c, m);
    } else {
        /* pass on un-handled messages */
        send_socket_message(c->server->sock, m);
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
        return send_socket_messagev(c->sock, c->server->host, NULL, NULL,
                ERR_NEEDMOREPARAMS, c->server->nick, "JOIN",
                "Not enough parameters", NULL);

    /* TODO: the first parameter (channel to join) can be a comma-separated
     * list
     */

    return client_join_channel(c, m->param[0]);
}

/* part the channel - only actually part it if this was the last client in
 * the channel
 */
static int handle_part(Client *c, const Message *m) {
    if(m->nparams < 1)
        return send_socket_messagev(c->sock, c->server->host, NULL, NULL,
                ERR_NEEDMOREPARAMS, c->server->nick, "PART",
                "Not enough parameters", NULL);

    /* TODO: the first parameter (channel to join) can be a comma-separated
     * list
     */

    return client_part_channel(c, m->param[0]);
}

/* change our nick unless this is the first NICK sent by this client, in which
 * case change his nick
 */
static int handle_nick(Client *c, const Message *m) {
    if(m->nparams < 1)
        return send_socket_messagev(c->sock, c->server->host, NULL, NULL,
                ERR_NEEDMOREPARAMS, c->server->nick, "NICK",
                "Not enough parameters", NULL);

    if(c->gotnick) {
        /* this is not the first nick supplied by this client: the user really
         * wants to change nick, so let's make it happen
         */
        /* TODO: accept the first nick we receive upon starting up instead of
         * renaming everyone to muxirc
         */
        send_socket_message(c->server->sock, m);
        return 0;
    } else {
        /* inform the client about what his nick really is */
        c->gotnick = 1;

        return send_socket_messagev(c->sock, m->param[0], NULL, NULL, CMD_NICK,
                c->server->nick, NULL);
    }
}

/* tell the client the welcome messages */
static int handle_user(Client *c, const Message *m) {
    int i;

    for(i = 0; i < c->server->nwelcomes; i++)
        if(send_socket_message(c->sock, c->server->welcomemsg[i]))
            break;

    /* find out the user modes */
    send_socket_messagev(c->server->sock, NULL, NULL, NULL, CMD_MODE,
            c->server->nick, NULL);

    /* request an MOTD for this client */
    c->motd_state = MOTD_WANT;

    return i != c->server->nwelcomes;
}

/* handle private messages by forwarding them to the server and to other
 * clients that are in the same channel
 */
static int handle_privmsg(Client *c, const Message *m) {
    if(m->nparams < 2)
        return send_socket_messagev(c->sock, c->server->host, NULL, NULL,
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
    send_socket_message(c->server->sock, m);

    return 0;
}

/* disconnect this client (actually just put it in the error state so that it
 * is disconnected at the next opportunity)
 */
static int handle_quit(Client *c, const Message *m) {
    c->sock->error = 1;
    return 0;
}
