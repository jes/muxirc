/* Socket handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef SOCKET_H_INC
#define SOCKET_H_INC

typedef struct Socket {
    int fd;
    int error;
    char buf[1024];
    size_t bytes;
} Socket;

Socket *new_socket(void);
int send_socket_string(Socket *sock, const char *str, ssize_t len);
int read_data(Socket *sock);

#endif
