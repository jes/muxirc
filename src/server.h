/* Server handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef STATE_H_INC
#define STATE_H_INC

typedef struct Server {
    int listenfd;
    int motd_state;
    char *nick;
    char *user;
    char *host;
    int nwelcomes;
    Message **welcomemsg;
    struct Socket *sock;
    struct Channel *channel_list;
    struct Client *client_list;
} Server;

void init_server_handlers(void);
void irc_connect(Server *s, const char *server, const char *serverport,
        const char *username, const char *realname, const char *listenport);
void handle_new_connection(Server *s);
void handle_server_data(Server *s);
int handle_server_message(Server *s, const struct Message *m);

#endif
