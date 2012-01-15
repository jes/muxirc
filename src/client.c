/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "message.h"
#include "server.h"
#include "client.h"

typedef int(*ClientMessageHandler)(Client *, const Message *);

static ClientMessageHandler message_handler[NCOMMANDS];

static int handle_join(Client *, const Message *);

/* initialise handler functions for client messages */
void init_client_handlers(void) {
    message_handler[CMD_JOIN] = handle_join;
}

/* return a new empty client */
Client *new_client(void) {
    Client *c = malloc(sizeof(Client));
    memset(c, 0, sizeof(Client));
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
 *  :muxirc <command> <params...>
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

/* handle a message from the given client (ignore any invalid ones) */
int handle_client_message(Client *c, const Message *m) {
    if(m->command >= 0 && m->command < NCOMMANDS
            && message_handler[m->command])
        return message_handler[m->command](c, m);
    else
        return 0;
}

/* join the channel */
static int handle_join(Client *c, const Message *m) {
    if(m->nparams < 1)
        return send_client_messagev(c, "muxirc", NULL, NULL,
                ERR_NEEDMOREPARAMS, c->server->nick, "JOIN",
                ":Not enough parameters", NULL);
    else
        return send_server_message(c->server, m);
}
