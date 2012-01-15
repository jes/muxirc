/* Channel handling for muxirc
 *
 * James Stanley 2012
 */

#include <string.h>
#include <stdlib.h>

#include "message.h"
#include "client.h"
#include "channel.h"
#include "server.h"

/* allocate a new empty channel */
Channel *new_channel(void) {
    Channel *chan = malloc(sizeof(Channel));
    memset(chan, 0, sizeof(Channel));
    return chan;
}

/* remove the given channel from it's list (if any) and free it */
void free_channel(Channel *c) {
}

/* prepend a client to the list pointed to by list, point list to c,
 * and return the new head
 */
Channel *prepend_channel(Channel *chan, Channel **list) {
    if(*list) {
        if((*list)->prev)
            (*list)->prev->next = chan;
        chan->prev = (*list)->prev;
        chan->next = *list;
        (*list)->prev = chan;
    } else {
        chan->prev = NULL;
        chan->next = NULL;
    }

    *list = chan;

    return *list;
}

/* add a client to the channel and return the channel */
Channel *add_channel_client(Channel *chan, Client *c) {
    chan->nclients++;

    /* if the new client count is a power of two, double the size to
     * allow more clients
     */
    if((m->nclients & (m->nclients - 1)) == 0)
        m->client = realloc(m->client, m->nclients * 2 * sizeof(Client *));

    m->client[m->nclients - 1] = c;

    return chan;
}

/* return the channel from the list with the given name, or NULL if there is
 * none
 */
Channel *lookup_channel(Channel *list, const char *channel) {
    Channel *chan;

    for(chan = list;
        chan && strcasecmp(chan->name, channel) != 0;
        chan = chan->next);

    return chan;
}

/* attempt to add c to the channel with the given name */
void client_join_channel(Client *c, const char *channel) {
    Channel *chan = lookup_channel(c->server->channel_list, channel);

    /* if the channel doesn't exist yet, ask to join it */
    if(!chan) {
        chan = new_channel();
        chan->name = strdup(channel);
        prepend_channel(chan, &(s->channel_list));
        chan->state = CHAN_JOINING;

        send_server_messagev(c->server, CMD_JOIN, channel, NULL);
    }

    /* if we are already in the channel, tell the client */
    if(chan->state == CHAN_JOINED)
        send_client_messagev(c, c->server->nick, c->server->user,
                c->server->host, CMD_JOIN, channel);
}

/* mark the channel with the given name as successfully joined and tell all
 * appropriate clients; m should contain the message received from the server
 * if available (if not, set it to NULL and an appropriate message will be
 * created instead)
 */
void joined_channel(Server *s, const char *channel, Message *m) {
    Channel *chan = lookup_channel(s->channel_list, channel);

    /* if the channel does not exist, make it
     * TODO: should this just instantly part instead?
     */
    if(!chan) {
        chan = new_channel();
        chan->name = strdup(channel);
        prepend_channel(chan, &(s->channel_list));
    }

    chan->state = CHAN_JOINED;

    char *strmsg;
    size_t msglen;

    /* construct a JOIN message if we were not given one */
    if(!m) {
        m = new_message();
        m->nick = s->nick;
        m->user = s->user;
        m->host = s->host;
        m->command = CMD_JOIN;
        add_message_param(m, strdup(channel));

        strmsg = strmessage(m, &msglen);

        free_message(m);
    }

    int i;
    for(i = 0; i < chan->nclients; i++)
        send_client_string(chan->client[i], strmsg, msglen);

    free(strmsg);
}
