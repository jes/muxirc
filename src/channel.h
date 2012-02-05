/* Channel handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef CHANNEL_H_INC
#define CHANNEL_H_INC

typedef struct Channel {
    char *name;
    char *topic;
    int state;
    struct Channel *prev, *next;
} Channel;

enum { CHAN_JOINING, CHAN_JOINED };

Channel *new_channel(void);
void free_channel(Channel *chan, Channel **list);
Channel *prepend_channel(Channel *chan, Channel **list);
Channel *lookup_channel(Channel *list, const char *channel);
void join_channel(Server *s, const char *channel);
void joined_channel(Server *s, const char *channel);
int part_channel(Server *s, const char *channel);
void parted_channel(Server *s, const char *channel);

#endif
