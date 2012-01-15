/* Channel handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef CHANNEL_H_INC
#define CHANNEL_H_INC

typedef struct Channel {
    char *name;
    int state;
    int nclients;
    struct Client **client;
    struct Channel *prev, *next;
} Channel;

enum { CHAN_JOINING, CHAN_JOINED };

Channel *new_channel(void);
void free_channel(Channel *chan);
Channel *prepend_channel(Channel *chan, Channel **list);
Channel *add_channel_client(Channel *chan, Client *c);
Channel *lookup_channel(Channel *list, const char *channel);
void client_join_channel(Client *c, const char *channel);
void joined_channel(Server *s, const char *channel, const Message *m);
void send_channel_string(Channel *chan, const char *str, ssize_t len);
void send_channel_message(Channel *chan, const Message *m);

#endif
