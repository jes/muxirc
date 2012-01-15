/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef CLIENT_H_INC
#define CLIENT_H_INC

typedef struct Client {
    int fd;
    int error;
    char registered;
    struct Server *server;
    struct Client *prev, *next;
} Client;

void init_client_handlers(void);
Client *new_client(void);
void free_client(Client *c);
Client *prepend_client(Client *c, Client **list);
int send_client_string(Client *c, const char *str, ssize_t len);
int send_client_message(Client *c, Message *m);
int send_client_messagev(Client *c, int command, ...);
int handle_client_message(Client *c, const struct Message *m);

#endif
