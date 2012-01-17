/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "message.h"
#include "server.h"
#include "client.h"
#include "channel.h"

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

/* handle a disconnection by a client by leaving any channels that client is
 * the sole client for, removing him from the list of clients, and freeing him
 */
void handle_client_disconnect(Client *c) {
    /* TODO: leave any channels he is the sole client for */

    printf("Handling client disconnect\n");

    close(c->fd);
    free_client(c);
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
                "Not enough parameters", NULL);
    else
        return client_join_channel(c, m->param[0]);
}
