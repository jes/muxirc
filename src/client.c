/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "state.h"
#include "message.h"
#include "client.h"

typedef int(*ClientMessageHandler)(Client *, const Message *);

static ClientMessageHandler message_handler[NCOMMANDS];

static int handler_ignore(Client *, const Message *);
static int handler_join(Client *, const Message *);

void init_client_handlers(void) {
    int i;

    /* by default, do nothing for all commands */
    for(i = 0; i < NCOMMANDS; i++)
        message_handler[i] = handler_ignore;

    message_handler[CMD_JOIN] = handler_join;
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

/* send an error message to the given client, in the form:
 *  :muxirc <error> <nick> <params...>
 */
static int send_error_message(Client *c, int error, ...) {
    va_list argp;
    Message *m = new_message();

    m->nick = strdup("muxirc");
    m->command = error;
    m->nparams = 1;
    m->param = malloc(sizeof(char*));
    m->param[0] = strdup(state.nick);

    va_start(argp, error);

    char *s;
    while(1) {
        s = va_arg(argp, char *);

        if(!s)
            break;

        m->nparams++;
        m->param = realloc(m->param, sizeof(char*));
        m->param[m->nparams - 1] = strdup(s);
    }

    va_end(argp);

    int r = send_message(c->fd, m);
    free_message(m);

    return r;
}

/* handle a message from the given client */
int handle_client_message(Client *c, const Message *m) {
    return message_handler[m->command](c, m);
}

/* ignore the message by doing nothing and returning success */
static int handler_ignore(Client *c, const Message *m) {
    return 0;
}

/* join the channel */
static int handler_join(Client *c, const Message *m) {
    if(m->nparams < 1)
        return send_error_message(c, ERR_NEEDMOREPARAMS, "JOIN",
                ":Not enough parameters", NULL);
    else
        return send_message(state.fd, m);
}
