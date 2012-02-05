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
void free_channel(Channel *chan, Channel **list) {
    free(chan->name);
    free(chan->topic);

    if(chan->prev)
        chan->prev->next = chan->next;
    if(chan->next)
        chan->next->prev = chan->prev;

    if(list && *list == chan)
        *list = chan->next;

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

/* attempt to join the channel */
void join_channel(Server *s, const char *channel) {
    Channel *chan = lookup_channel(s->channel_list, channel);

    /* if the channel doesn't exist yet, make it */
    if(!chan) {
        chan = new_channel();
        chan->name = strdup(channel);
        prepend_channel(chan, &(s->channel_list));
        chan->state = CHAN_JOINING;
    }

    /* send a (possibly duplicate) join message if we've not joined yet */
    if(chan->state != CHAN_JOINED)
        send_socket_messagev(s->sock, NULL, NULL, NULL, CMD_JOIN,
                channel, NULL);
}

/* mark the channel with the given name as successfully joined */
void joined_channel(Server *s, const char *channel) {
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
}

/* attempt to part the channel */
int part_channel(Server *s, const char *channel) {
    return send_socket_messagev(s->sock, s->nick, s->user,
            s->host, CMD_PART, channel, NULL);
}

/* we have parted, delete the channel */
void parted_channel(Server *s, const char *channel) {
    Channel *chan = lookup_channel(s->channel_list, channel);

    /* if the channel exists, delete it */
    if(chan)
        free_channel(chan, &(s->channel_list));
}
