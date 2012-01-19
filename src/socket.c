/* Socket handling for muxirc
 *
 * James Stanley 2012
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "socket.h"

/* allocate a socket for fd -1 */
Socket *new_socket(void) {
    Socket *s = malloc(sizeof(Socket));
    memset(s, 0, sizeof(Socket));
    s->fd = -1;
    return s;
}

/* send the given string to the given socket, returning -1 on error and 0
 * on success; if len >= 0 it should contain the length of str, otherwise
 * strlen(str) will be used
 * this function updates the socket error state
 */
int send_socket_string(Socket *sock, const char *str, ssize_t len) {
    ssize_t r;

    if(len < 0)
        len = strlen(str);

    printf("Sending: ##%s##\n", str);

    /* keep trying while EINTR */
    while((r = write(sock->fd, str, len)) < 0)
        if(r != EINTR)
            break;

    sock->error = r < 0 ? -1 : 0;

    return sock->error;
}

/* read data from the file descriptor, appending it to the buffer, updating
 * *bufused to indicate how much is now used, and without going over the buflen
 * limit; return 0 on success and -1 on error
 */
int read_data(Socket *sock) {
    ssize_t r;

    /* keep reading until it is successful or the error is not EINTR */
    while((r = read(sock->fd, sock->buf + sock->bytes, 1023 - sock->bytes)) < 0)
        if(errno != EINTR)
            break;

    /* return an error if there is an error */
    if(r <= 0) {
        if(r < 0)
            perror("read");
        sock->error = -1;
        return -1;
    }

    /* update *bufused and nul-terminate buf */
    sock->bytes += r;
    sock->buf[sock->bytes] = '\0';

    printf("Read: %s", sock->buf);

    return 0;
}
