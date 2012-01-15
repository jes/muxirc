/* Message handling for muxirc
 *
 * James Stanley 2012
 */

#ifndef MESSAGE_H_INC
#define MESSAGE_H_INC

typedef struct Message {
    char *nick, *user, *host;
    int command;
    char **param;
    int nparams;
} Message;

enum {
    CMD_NONE=0,
    ERR_NEEDMOREPARAMS=461,
    CMD_PASS=1000, CMD_NICK, CMD_USER, CMD_SERVER, CMD_OPER, CMD_QUIT,
    CMD_SQUIT, CMD_JOIN, CMD_PART, CMD_MODE, CMD_TOPIC, CMD_NAMES, CMD_LIST,
    CMD_INVITE, CMD_KICK, CMD_VERSION, CMD_STATS, CMD_LINKS, CMD_TIME,
    CMD_CONNECT, CMD_TRACE, CMD_ADMIN, CMD_INFO, CMD_PRIVMSG, CMD_NOTICE,
    CMD_WHO, CMD_WHOIS, CMD_WHOWAS, CMD_KILL, CMD_PING, CMD_PONG, CMD_ERROR,
    CMD_AWAY, CMD_REHASH, CMD_RESTART, CMD_SUMMON, CMD_USERS, CMD_WALLOPS,
    CMD_USERHOST, CMD_ISON,
    NCOMMANDS
};

extern char *command_string[];

Message *new_message(void);
void free_message(Message *m);
void free_message_params(Message *m);
Message *add_message_param(Message *m, char *s);
Message *parse_message(const char *line);
int parse_prefix(const char **line, Message *m);
int parse_command(const char **line, Message *m);
int parse_params(const char **line, Message *m);
void skip_space(const char **p);
char *strmessage(const Message *m, size_t *length);
int send_string(int fd, const char *str, ssize_t len);
int send_message(int fd, const Message *m);

#endif
