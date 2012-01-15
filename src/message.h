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
    CMD_NONE=0, CMD_PASS, CMD_NICK, CMD_USER, CMD_SERVER, CMD_OPER, CMD_QUIT,
    CMD_SQUIT, CMD_JOIN, CMD_PART, CMD_MODE, CMD_TOPIC, CMD_NAMES, CMD_LIST,
    CMD_INVITE, CMD_KICK, CMD_VERSION, CMD_STATS, CMD_LINKS, CMD_TIME,
    CMD_CONNECT, CMD_TRACE, CMD_ADMIN, CMD_INFO, CMD_PRIVMSG, CMD_NOTICE,
    CMD_WHO, CMD_WHOIS, CMD_WHOWAS, CMD_KILL, CMD_PING, CMD_PONG, CMD_ERROR,
    CMD_AWAY, CMD_REHASH, CMD_RESTART, CMD_SUMMON, CMD_USERS, CMD_WALLOPS,
    CMD_USERHOST, CMD_ISON,
    NCOMMANDS,
    ERR_NEEDMOREPARAMS=461
};

extern char *command_string[];

Message *new_message(void);
void free_message(Message *m);
Message *parse_message(const char *line);
int parse_prefix(const char **line, Message *m);
int parse_command(const char **line, Message *m);
int parse_params(const char **line, Message *m);
void skip_space(const char **p);
int send_message(int fd, const Message *m);

#endif
