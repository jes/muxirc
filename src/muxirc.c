/* muxirc - multiplex several irc clients into one nick
 *
 * James Stanley 2012
 */

#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "message.h"
#include "client.h"
#include "server.h"

/* something terrible has happened; write a message to the console, quit the
 * server, and inform all of the clients
 */
void fatal(Server *s, const char *prefix, const char *msg) {
    Message *m;
    char text[512];

    fprintf(stderr, "%s: %s\n", prefix, msg);

    snprintf(text, 512, "%s: %s", prefix, msg);

    send_server_messagev(s, CMD_QUIT, text);
    close(s->serverfd);
    s->serverfd = -1;

    m = new_message();
    m->nick = strdup("muxirc");
    m->command = CMD_ERROR;
    add_message_param(m, strdup(text));

    size_t msglen;
    char *strmsg = strmessage(m, &msglen);

    Client *c;
    for(c = s->client_list; c; c = c->next)
        send_client_string(c, strmsg, msglen);

    free(strmsg);
    free_message(m);

    exit(1);
}

int main() {
    Server serverstate;

    init_client_handlers();
    init_server_handlers();

    /* TODO: take these from arguments */
    irc_connect(&serverstate, "localhost", "6667", "muxirc",
            "IRC Multiplexer", "muxirc");

    while(1) {
        struct pollfd fd[1];
        int i = 0;

        /* fd[0] - connection to server */
        fd[i].fd = serverstate.serverfd;
        fd[i].events = POLLIN;
        fd[i].revents = 0;
        i++;
        /* TODO: fd[1] - listening socket */
        /* TODO: fd[2..] - connections to clients */

        int n = poll(fd, i, -1);

        if(n == -1) {
            fatal(&serverstate, "muxirc: poll", strerror(errno));
        } else if(n == 0) {
            fatal(&serverstate, "muxirc: poll", "unexpected timeout");
        } else {
            /* data/disconnection from server */
            if(fd[0].revents & POLLIN)
                handle_server_data(&serverstate);
            if(fd[0].revents & POLLHUP)
                fatal(&serverstate, "muxirc", "upstream disconnect (hup)");
        }

        /* check server for errors */
        if(serverstate.error)
            fatal(&serverstate, "muxirc", "upstream disconnect (error)");

        /* check clients for errors */
        Client *c, *c_next;
        for(c = serverstate.client_list; c; c = c_next) {
            c_next = c->next;

            if(c->error)
                disconnect_client(c);
        }
    }
}
