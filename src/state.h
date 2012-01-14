/* State handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef STATE_H_INC
#define STATE_H_INC

typedef struct State {
    int fd;
    const char *nick;
    struct Channel *channel_list;
} State;

extern State state;

#endif
