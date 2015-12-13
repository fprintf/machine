#ifndef IRCMSG_HEADER__H_
#define IRCMSG_HEADER__H_

#include <stdio.h> /* FILE * */

enum ircmsg_type {
    IRC_UNSET,
    IRC_INIT,         /* Init event, special user event called once per startup of program */
    IRC_PING,
    IRC_PRIVMSG,
    IRC_NICK,
    IRC_NOTICE,
    IRC_MODE,
    IRC_KICK,
    IRC_JOIN,
    IRC_PART,
    IRC_QUIT,
    IRC_TOPIC,
    IRC_NUMERIC,
};

struct ircmsg_api {
    /* Create/Destroy ircmsg object */
    struct ircmsg * (*parse)(const char * line);
    void (*free)(struct ircmsg * msg);

    /* Get */
    enum ircmsg_type (*type)(struct ircmsg * msg);
    char * (*command)(struct ircmsg * msg);
    char * (*text)(struct ircmsg * msg);
    char * (*nick)(struct ircmsg * msg);
    char * (*real)(struct ircmsg * msg);
    char * (*host)(struct ircmsg * msg);
	char * (*servername)(struct ircmsg * msg); /* Identifies the name of the server this message came from */
	struct server * (*server)(struct ircmsg * msg); /* Returns the global config object server this message is associated with */
    short (*numeric)(struct ircmsg * msg);
    /* Get parameter at index 'argc' */
    char * (*argv)(struct ircmsg * msg, int argc);

    /* Dump to FILE * fp (debugging use.. ) */
    void (*fdump)(struct ircmsg * msg, FILE * fp);
};

extern const struct ircmsg_api Msg;

#endif
