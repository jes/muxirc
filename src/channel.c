/* Channel handling for muxirc
 *
 * James Stanley 2012
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "socket.h"
#include "message.h"
#include "client.h"
#include "server.h"
#include "channel.h"

/* allocate a new empty channel */
Channel *new_channel(void) {
    Channel *chan = malloc(sizeof(Channel));
    memset(chan, 0, sizeof(Channel));
    return chan;
}

/* remove the given channel from it's list (if any) and free it */
void free_channel(Channel *chan) {
    free(chan->name);
    free(chan->client);

    if(chan->prev)
        chan->prev->next = chan->next;
    if(chan->next)
        chan->next->prev = chan->prev;

    free(chan);
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
    if((chan->nclients & (chan->nclients - 1)) == 0)
        chan->client = realloc(chan->client,
                chan->nclients * 2 * sizeof(Client *));

    chan->client[chan->nclients - 1] = c;

    return chan;
}

/* return the channel from the list with the given name, or NULL if there is
 * none
 */
Channel *lookup_channel(Channel *list, const char *channel) {
    Channel *chan;

    for(chan = list;
        chan && strcasecmp(chan->name, channel) != 0;
        chan = chan->next)
        printf("  channel not %s!\n", chan->name);

    return chan;
}

/* remove this client from all of the channels he is in */
void remove_client_from_channels(Client *c) {
    Channel *chan, *chan_next;

    for(chan = c->server->channel_list; chan; chan = chan_next) {
        chan_next = chan->next;
        remove_client_from_channel(c, chan);
    }
}

/* remove this client from this channel, deleting the channel if necessary */
void remove_client_from_channel(Client *c, Channel *chan) {
    int i;

    /* find the index of this client */
    for(i = 0; i < chan->nclients && chan->client[i] != c; i++);

    if(i == chan->nclients) {
        fprintf(stderr, "warning: tried to remove a client from a channel he "
                "does not appear to be in!\n");
        return;
    }

    if(chan->nclients > 1) {
        /* just remove this client */
        chan->nclients--;
        for(; i < chan->nclients; i++)
            chan->client[i] = chan->client[i+1];
        chan->client = realloc(chan->client,
                sizeof(Client *) * chan->nclients);
    } else {
        /* tell the server */
        send_socket_messagev(c->server->sock, NULL, NULL, NULL, CMD_PART,
                chan->name, NULL);

        /* remove this channel entirely */
        free_channel(chan);
    }
}

/* attempt to add c to the channel with the given name; return 0 on
 * successfully informing the client that he is joined (or not having to), and
 * non-zero on failure
 */
int client_join_channel(Client *c, const char *channel) {
    Channel *chan = lookup_channel(c->server->channel_list, channel);

    printf("Attempting to join %s\n", channel);

    /* if the channel doesn't exist yet, ask to join it */
    if(!chan) {
        chan = new_channel();
        chan->name = strdup(channel);
        prepend_channel(chan, &(c->server->channel_list));
        chan->state = CHAN_JOINING;

        send_socket_messagev(c->server->sock, NULL, NULL, NULL, CMD_JOIN,
                channel, NULL);
    }

    /* add this client to the channel, whatever state it is in */
    add_channel_client(chan, c);

    /* if we haven't joined yet, don't do anything else */
    if(chan->state != CHAN_JOINED)
        return 0;

    /* TODO: mark this client as requesting topic and names for this
     * channel
     */
    send_socket_messagev(c->server->sock, NULL, NULL, NULL, CMD_TOPIC, channel,
            NULL);
    send_socket_messagev(c->server->sock, NULL, NULL, NULL, CMD_NAMES, channel,
            NULL);

    return send_socket_messagev(c->sock, c->server->nick, c->server->user,
            c->server->host, CMD_JOIN, channel, NULL);
}

/* attempt to remove c from the channel with the given name; return 0 on
 * successfully informing the client that he has parted and 0 on failure;
 * actually part the channel if this is the only client;
 */
int client_part_channel(Client *c, const char *channel) {
    Channel *chan = lookup_channel(c->server->channel_list, channel);

    int i = -1;
    if(chan)
        for(i = 0; i < chan->nclients && chan->client[i] != c; i++);

    /* if the channel doesn't exist yet or the user is not in it, tell him
     * this.
     */
    if(!chan || i == chan->nclients)
        return send_socket_messagev(c->sock, c->server->host, NULL, NULL,
                ERR_NOTONCHANNEL, c->server->nick, channel,
                "You're not on that channel", NULL);

    /* remove him from the channel */
    remove_client_from_channel(c, chan);

    /* tell him he has parted */
    return send_socket_messagev(c->sock, c->server->nick, c->server->user,
            c->server->host, CMD_PART, channel, NULL);
}

/* mark the channel with the given name as successfully joined and tell all
 * appropriate clients; m should contain the message received from the server
 * if available (if not, set it to NULL and an appropriate message will be
 * created instead)
 */
void joined_channel(Server *s, const char *channel, const Message *m) {
    Channel *chan = lookup_channel(s->channel_list, channel);

    fprintf(stderr, "Joined channel %s\n", channel);

    /* if the channel does not exist, make it
     * TODO: should this just instantly part instead?
     */
    if(!chan) {
        printf("chan no existe!\n");
        chan = new_channel();
        chan->name = strdup(channel);
        prepend_channel(chan, &(s->channel_list));
    }

    chan->state = CHAN_JOINED;

    /* don't bother constructing a message if there are no clients */
    /* TODO: should we instantly part instead? */
    if(chan->nclients == 0)
        return;

    char *strmsg;
    size_t msglen;

    /* construct a JOIN message if we were not given one */
    if(!m) {
        Message *msg = new_message();
        msg->nick = strdup(s->nick);
        if(s->user)
            msg->user = strdup(s->user);
        if(s->host)
            msg->host = strdup(s->host);
        msg->command = CMD_JOIN;
        add_message_param(msg, strdup(channel));

        strmsg = strmessage(msg, &msglen);

        free_message(msg);
    } else {
        strmsg = strmessage(m, &msglen);
    }

    int i;
    for(i = 0; i < chan->nclients; i++)
        send_socket_string(chan->client[i]->sock, strmsg, msglen);

    free(strmsg);
}

/* send a string to all clients in the channel */
void send_channel_string(Channel *chan, Client *except, const char *str,
        ssize_t len) {
    int i;
    for(i = 0; i < chan->nclients; i++)
        if(chan->client[i] != except)
            send_socket_string(chan->client[i]->sock, str, len);
}

/* send a message to all clients in this channel */
void send_channel_message(Channel *chan, Client *except, const Message *m) {
    size_t msglen;
    char *strmsg = strmessage(m, &msglen);

    send_channel_string(chan, except, strmsg, msglen);
}

/* send a message to the given channel, in the form:
 *  :nick!user@host <command> <params...>
 */
void send_channel_messagev(Channel *chan, Client *except, const char *nick,
        const char *user, const char *host, int command, ...) {
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

    send_channel_message(chan, except, m);
    free_message(m);
}
