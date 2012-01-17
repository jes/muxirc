/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef CLIENT_H_INC
#define CLIENT_H_INC

typedef struct Client {
    int fd;
    int error;
    int motd_state;
    char buf[1024];
    int gotnick;
    size_t bytes;
    struct Server *server;
    struct Client *prev, *next;
} Client;

enum {
    MOTD_HAPPY=0, MOTD_WANT, MOTD_READING
};

void init_client_handlers(void);
Client *new_client(void);
void free_client(Client *c);
Client *prepend_client(Client *c, Client **list);
void disconnect_client(Client *c);
int send_client_string(Client *c, const char *str, ssize_t len);
int send_client_message(Client *c, const struct Message *m);
int send_client_messagev(Client *c, const char *nick, const char *user,
        const char *host, int command, ...);
void handle_client_data(Client *c);
int handle_client_message(Client *c, const struct Message *m);

#endif
