/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef CLIENT_H_INC
#define CLIENT_H_INC

typedef struct Client {
    int motd_state;
    int gotnick;
    int authd;
    char *pass;
    struct Socket *sock;
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
void handle_client_data(Client *c);
int handle_client_message(Client *c, const struct Message *m);

#endif
