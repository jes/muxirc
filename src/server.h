/* Server handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef STATE_H_INC
#define STATE_H_INC

typedef struct Server {
    int serverfd;
    int listenfd;
    int error;
    char *nick;
    char *user;
    char *host;
    int nwelcomes;
    char **welcome_line;
    char buf[1024];
    size_t bytes;
    struct Channel *channel_list;
    struct Client *client_list;
} Server;

void init_server_handlers(void);
void irc_connect(Server *s, const char *server, const char *serverport,
        const char *username, const char *realname, const char *nick,
        const char *listenport);
int send_server_string(Server *s, const char *str, ssize_t len);
int send_server_message(Server *s, const struct Message *m);
int send_server_messagev(Server *s, int command, ...);
void handle_new_connection(Server *s);
void handle_server_data(Server *s);
int handle_server_message(Server *s, const struct Message *m);

#endif
