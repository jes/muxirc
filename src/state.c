/* State handling for muxirc
 *
 * James Stanley 2012
 */

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "message.h"
#include "state.h"

State state;

/* connect to irc and initialise the state */
void irc_connect(const char *server, const char *port, const char *username,
        const char *realname, const char *nick) {
    struct addrinfo hints, *servinfo, *p;
    int n;
    int fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((n = getaddrinfo(server, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(n));
        exit(1);
    }

    /* find a working connection */
    for(p = servinfo; p; p = p->ai_next) {
        if((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if(connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            perror("connect");
            continue;
        }

        /* successfully socket'd and connect'd, use this fd */
        break;
    }

    if(!p) {
        fprintf(stderr, "error: failed to connect to %s:%s\n",
                server, port);
        exit(1);
    }

    freeaddrinfo(servinfo);

    state.fd = fd;
    state.nick = nick;
    state.channel_list = NULL;

    Message *m = new_message();
    m->param = malloc(4 * sizeof(char *));

    /* TODO: Send PASS if necessary */
    m->command = CMD_NICK;
    m->nparams = 1;
    m->param[0] = strdup(nick);
    send_message(fd, m);
    free(m->param[0]);

    m->command = CMD_USER;
    m->nparams = 4;
    m->param[0] = strdup(username);
    m->param[1] = strdup("hostname");/* TODO: this */
    m->param[2] = strdup(server);
    m->param[3] = strdup(realname);
    send_message(fd, m);

    free_message(m);
}
