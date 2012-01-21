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

typedef int(*GenericMessageHandler)(void *, const Message *);

enum {
    CMD_NONE=0,
    RPL_WELCOME=1, RPL_YOURHOST, RPL_CREATED, RPL_MYINFO, RPL_ISUPPORT,
    RPL_TOPIC=332, RPL_TOPICWHOTIME, RPL_MOTD=372, RPL_MOTDSTART=375,
    RPL_ENDOFMOTD,
    ERR_NOTONCHANNEL=442, ERR_NEEDMOREPARAMS=461,
    CMD_INVALID=1000,
    FIRST_CMD=1001,
    CMD_PASS=1001, CMD_NICK, CMD_USER, CMD_SERVER, CMD_OPER, CMD_QUIT,
    CMD_SQUIT, CMD_JOIN, CMD_PART, CMD_MODE, CMD_TOPIC, CMD_NAMES, CMD_LIST,
    CMD_INVITE, CMD_KICK, CMD_VERSION, CMD_STATS, CMD_LINKS, CMD_TIME,
    CMD_CONNECT, CMD_TRACE, CMD_ADMIN, CMD_INFO, CMD_PRIVMSG, CMD_NOTICE,
    CMD_WHO, CMD_WHOIS, CMD_WHOWAS, CMD_KILL, CMD_PING, CMD_PONG, CMD_ERROR,
    CMD_AWAY, CMD_REHASH, CMD_RESTART, CMD_SUMMON, CMD_USERS, CMD_WALLOPS,
    CMD_USERHOST, CMD_ISON, CMD_CAP, CMD_MOTD,
    NCOMMANDS
};

extern char *command_string[];

Message *new_message(void);
Message *copy_message(const Message *m);
void free_message(Message *m);
void free_message_params(Message *m);
Message *add_message_param(Message *m, char *s);
Message *parse_message(const char *line);
int parse_prefix(const char **line, Message *m);
int parse_command(const char **line, Message *m);
int parse_params(const char **line, Message *m);
void skip_space(const char **p);
char *strmessage(const Message *m, size_t *length);
int send_socket_message(Socket *sock, const Message *m);
int send_socket_messagev(Socket *sock, const char *nick, const char *user,
        const char *host, int command, ...);
void handle_messages(char *buf, size_t *bufused, GenericMessageHandler handler,
        void *data);

#endif
