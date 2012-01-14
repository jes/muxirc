/* Channel handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef CHANNEL_H_INC
#define CHANNEL_H_INC

typedef struct Channel {
    int nclients;
    struct Client **client;
    char *name;
    struct Channel *prev, *next;
} Channel;

Channel *new_channel(void);

#endif
