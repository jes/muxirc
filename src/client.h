/* Client handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef CLIENT_H_INC
#define CLIENT_H_INC

typedef struct Client {
    int fd;
    char *name;
    struct sockaddr_storage addr;
    struct Client *prev, *next;
} Client;

#endif
