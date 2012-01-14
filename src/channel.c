/* Channel handling for muxirc
 *
 * James Stanley 2012
 */

#include <string.h>
#include <stdlib.h>

#include "message.h"
#include "client.h"
#include "channel.h"
#include "state.h"

/* allocate a new empty channel */
Channel *new_channel(void) {
    Channel *chan = malloc(sizeof(Channel));
    memset(chan, 0, sizeof(Channel));
    return chan;
}

/* attempt to add c to the channel with the given name */
void join_channel(Client *c, const char *channel) {
}
