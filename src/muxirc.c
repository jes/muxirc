/* muxirc - multiplex several irc clients into one nick
 *
 * James Stanley 2012
 */

#include <stdio.h>
#include <poll.h>
#include <string.h>

#include "message.h"
#include "client.h"
#include "state.h"

int main() {
    init_client_handlers();

    /* TODO: take these from arguments */
    irc_connect("irc.freenode.net", "6667", "muxirc", "IRC Multiplexer",
            "muxirc");

    while(1) {
        struct pollfd fd[1];
        int i = 0;

        fd[i].fd = state.fd;
        fd[i].events = POLLIN;

        /* TODO: add client fds */

        /* TODO: something sane when connection times out */
        int n = poll(fd, i, -1);

        /* TODO: handle data */
    }
}
