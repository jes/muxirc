/* Message handling for muxirc
 *
 * James Stanley 2012
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "message.h"
#include "str.h"

char *command_string[] = {
    "PASS", "NICK", "USER", "SERVER", "OPER", "QUIT",
    "SQUIT", "JOIN", "PART", "MODE", "TOPIC", "NAMES", "LIST",
    "INVITE", "KICK", "VERSION", "STATS", "LINKS", "TIME",
    "CONNECT", "TRACE", "ADMIN", "INFO", "PRIVMSG", "NOTICE",
    "WHO", "WHOIS", "WHOWAS", "KILL", "PING", "PONG", "ERROR",
    "AWAY", "REHASH", "RESTART", "SUMMON", "USERS", "WALLOPS",
    "USERHOST", "ISON",
    NULL
};

/* allocate an empty Message */
Message *new_message(void) {
    Message *m = malloc(sizeof(Message));
    memset(m, 0, sizeof(Message));
    return m;
}

/* free the given message and all pointer fields */
void free_message(Message *m) {
    free(m->nick);
    free(m->user);
    free(m->host);
    free_message_params(m);
    free(m);
}

/* free the parameters for the message */
void free_message_params(Message *m) {
    int i;

    for(i = 0; i < m->nparams; i++)
        free(m->param[i]);
    free(m->param);

    m->nparams = 0;
    m->param = NULL;
}

/* add a parameter to m containing s (NOT a copy), and return m
 * ** NOTE: this means that calling free_message_params will free s so
 * ** make sure you give a copy of s if that is necessary!
 */
Message *add_message_param(Message *m, char *s) {
    m->nparams++;

    /* if the new parameter count is a power of two, double the size to
     * allow more parameters
     */
    if((m->nparams & (m->nparams - 1)) == 0)
        m->param = realloc(m->param, m->nparams * 2 * sizeof(char *));

    m->param[m->nparams - 1] = s;

    return m;
}

/* return a Message structure representing the line, or NULL if a parse error
 * occurs.
 */
Message *parse_message(const char *line) {
    const char *p = line;
    Message *m = new_message();

    /* accept empty messages */
    if(*line == '\r' || *line == '\n')
        return m;

    if(parse_prefix(&p, m) != 0)
        goto cleanup;

    if(parse_command(&p, m) != 0)
        goto cleanup;

    if(parse_params(&p, m) != 0)
        goto cleanup;

    /* if this has not used up all of the line, fail */
    if(*p)
        goto cleanup;

    return m;

cleanup:
    free_message(m);
    return NULL;
}

/* parse a prefix from line and stick it in m, updating line to point to the
 * next text; return 0 on success and non-zero on failure
 */
int parse_prefix(const char **line, Message *m) {
    /* prefixes are optional */
    if(**line != ':')
        return 0;

    /* valid formats for a prefix are:
     *  :nick
     *  :nick!user
     *  :nick@host
     *  :nick!user@host
     */

    /* skip the colon */
    (*line)++;

    int nicklen = strcspn(*line, "!@ ");
    m->nick = strprefix(*line, nicklen);
    *line += nicklen + 1;

    /* copy user if there is one */
    if(*(*line-1) == '!') {
        int userlen = strcspn(*line, "@ ");
        m->user = strprefix(*line, userlen);
        *line += userlen + 1;
    }

    /* copy the host if there is one */
    if(*(*line-1) == '@') {
        int hostlen = strcspn(*line, " ");
        m->host = strprefix(*line, hostlen);
        *line += hostlen + 1;
    }

    skip_space(line);
    return 0;
}

/* parse a command from line and stick it in m, updating line to point to the
 * next text; return 0 on success and non-zero on failure
 */
int parse_command(const char **line, Message *m) {
    if(isdigit(**line)) {
        /* numeric command */
        if(!isdigit(*(*line + 1)) || !isdigit(*(*line + 2))
                || *(*line + 3) != ' ')
            return -1;
        
        m->command = atoi(*line);
        *line += 3;
    } else {
        /* textual command */
        int i;
        int commandlen = strcspn(*line, " ");
        for(i = 0; command_string[i]; i++) {
            if(commandlen == strlen(command_string[i])
                    && strncmp(*line, command_string[i], commandlen) == 0)
                break;
        }

        if(command_string[i])
            m->command = FIRST_CMD + i;
        else
            m->command = CMD_INVALID;

        *line += commandlen;
    }

    skip_space(line);
    return 0;
}

/* parse a parameter list from line and stick it in m, updating line to point
 * to the next text (end of line); return 0 on success and non-zero on
 * failure
 */
int parse_params(const char **line, Message *m) {
    while(**line && **line != '\r' && **line != '\n') {
        int paramlen;

        /* consume to end of line if this parameter begins with ":" */
        if(**line == ':')
            paramlen = strcspn(++(*line), "\r\n");
        else
            paramlen = strcspn(*line, " \r\n");

        add_message_param(m, strprefix(*line, paramlen));

        *line += paramlen;
        skip_space(line);
    }

    /* eat the \r\n */
    while(**line == '\r' || **line == '\n')
        (*line)++;

    return 0;
}

/* skip over space by advancing the pointer to the next non-space character. */
void skip_space(const char **p) {
    while(**p == ' ')
        (*p)++;
}

/* allocate a 513-byte buffer containing the stringified message, \r\n and a
 * nul byte, storing the full length of text (including \r\n) in *length if
 * length is not NULL
 */
char *strmessage(const Message *m, size_t *length) {
    char command[16];
    char *line = malloc(513);
    char *endptr = line;

    if(m->nick) {
        strappend(line, &endptr, 511, ":");
        strappend(line, &endptr, 511, m->nick);
        if(m->user) {
            strappend(line, &endptr, 511, "!");
            strappend(line, &endptr, 511, m->user);
        }
        if(m->host) {
            strappend(line, &endptr, 511, "@");
            strappend(line, &endptr, 511, m->host);
        }

        strappend(line, &endptr, 511, " ");
    }

    if(m->command < FIRST_CMD || m->command >= NCOMMANDS) {
        snprintf(command, 8, "%03d", m->command);
        strappend(line, &endptr, 511, command);
    } else {
        strappend(line, &endptr, 511, command_string[m->command - FIRST_CMD]);
    }

    int i;
    for(i = 0; i < m->nparams; i++) {
        strappend(line, &endptr, 511, " ");
        if(i == m->nparams-1 && strchr(m->param[i], ' '))
            strappend(line, &endptr, 511, ":");
        strappend(line, &endptr, 511, m->param[i]);
    }

    strappend(line, &endptr, 513, "\r\n");

    if(length)
        *length = endptr - line;

    return line;
}

/* send the given string to the given socket, returning -1 on error and 0
 * on success; if len >= 0 it should contain the length of str, otherwise
 * strlen(str) will be used
 */
int send_string(int fd, const char *str, ssize_t len) {
    ssize_t r;

    printf("Sending: ##%s##\n", str);

    /* keep trying while EINTR */
    while((r = write(fd, str, len)) < 0)
        if(r != EINTR)
            break;

    return r < 0 ? -1 : 0;
}

/* send the given message to the given socket, returning -1 on error and 0
 * on success
 */
int send_message(int fd, const Message *m) {
    size_t length;
    char *msg = strmessage(m, &length);

    ssize_t r = send_string(fd, msg, length);

    free(msg);
    return r;
}

/* read data from the file descriptor, appending it to the buffer, updating
 * *bufused to indicate how much is now used, and without going over the buflen
 * limit; return 0 on success and -1 on error
 */
int read_data(int fd, char *buf, size_t *bufused, size_t buflen) {
    ssize_t r;

    /* keep reading until it is successful or the error is not EINTR */
    while((r = read(fd, buf + *bufused, buflen - 1 - *bufused)) < 0)
        if(errno != EINTR)
            break;

    /* return an error if there is an error */
    if(r <= 0) {
        if(r < 0)
            perror("read");
        return -1;
    }

    /* update *bufused and nul-terminate buf */
    *bufused += r;
    buf[*bufused] = '\0';

    printf("Read: %s", buf);

    return 0;
}

/* handle messages from the string by parsing them and passing them to the
 * handler function, and removing all data that was handled (moving anything
 * left over to the start of buf)
 */
void handle_messages(char *buf, size_t *bufused, GenericMessageHandler handle,
        void *data) {
    char *p, *str = buf;

    /* while there are endlines, handle data */
    while((p = strpbrk(str, "\r\n"))) {
        char c = *p;
        *p = '\0';

        /* parse and handle the message */
        Message *m = parse_message(str);
        if(m) {
            handle(data, m);
            free_message(m);
        }

        *p = c;

        /* advance past the endline characters */
        str = p + strspn(p, "\r\n");
    }

    /* move any left-over data to the start of the buffer */
    *bufused -= str - buf;
    memmove(buf, str, *bufused + 1);
}
