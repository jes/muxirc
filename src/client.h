/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef CLIENT_H_INC
#define CLIENT_H_INC

typedef struct Client {
    int fd;
    char registered;
    char disconnected;
    struct Client *prev, *next;
} Client;

void init_client_handlers(void);
Client *new_client(void);
void free_client(Client *c);
Client *prepend_client(Client *c, Client **list);
int handle_client_message(Client *c, const struct Message *m);

#endif
