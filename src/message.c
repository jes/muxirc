/* Message handling for muxirc
 *
 * James Stanley 2012
 */

#include <stdlib.h>
#include <string.h>

#include "message.h"
#include "str.h"

char *command_string[] = {
    "PASS", "NICK", "USER", "SERVER", "OPER", "QUIT",
    "SQUIT", "JOIN", "PART", "MODE", "TOPIC", "NAMES", "LIST",
    "INVITE", "KICK", "VERSION", "STATS", "LINKS", "TIME",
    "CONNECT", "TRACE", "ADMIN", "INFO", "PRIVMSG", "NOTICE",
    "AWAY", "REHASH", "RESTART", "SUMMON", "USERS", "WALLOPS",
    "USERHOST", "ISON", NULL
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

    int i;
    for(i = 0; i < m->nparams; i++)
        free(m->param[i]);
    free(m->param);

    free(m);
}

/* return a Message structure representing the line, or NULL if a parse error
 * occurs.
 */
Message *parse_message(const char *line) {
    const char *p = line;
    Message *m = new_message();

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
        for(i = 0; i < NCOMMANDS; i++) {
            if(strncmp(*line, command_string[i], commandlen) == 0)
                break;
        }

        if(i == NCOMMANDS) /* no commands matched */
            return -1;

        m->command = i;
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
            paramlen = strcspn(*line, "\r\n");
        else
            paramlen = strcspn(*line, " \r\n");

        m->nparams++;
        m->param = realloc(m->param, m->nparams * sizeof(char *));
        m->param[m->nparams - 1] = strprefix(*line, paramlen);
        *line += paramlen + 1;
    }

    return 0;
}

/* skip over space by advancing the pointer to the next non-space character. */
void skip_space(const char **p) {
    while(**p == ' ')
        (*p)++;
}
