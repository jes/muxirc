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

#endif
