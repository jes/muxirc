/* Server handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef STATE_H_INC
#define STATE_H_INC

typedef struct Server {
    int fd;
    int error;
    const char *nick;
    const char *user;
    const char *host;
    struct Channel *channel_list;
    struct Client *client_list;
} Server;

void init_server_handlers(void);
void irc_connect(Server *s, const char *server, const char *port,
        const char *username, const char *realname, const char *nick);
int send_server_string(Server *s, const char *str, ssize_t len);
int send_server_message(Server *s, const struct Message *m);
int send_server_messagev(Server *s, int command, ...);
int handle_server_message(Server *s, const struct Message *m);

#endif
