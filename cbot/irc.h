#ifndef IRC_HEADER__H_
#define IRC_HEADER__H_

#include "con.h"

/* Interface */
extern const struct irc_api irc;

struct irc_api {
    struct irc * (*dispatch)(const char * line, struct con * con);
    void (*free)(struct irc * irc);

    /* Get */
    char * (*target)(struct irc * irc); /* target the message was for */
    char * (*text)(struct irc * irc);  /* returns actual text of this message if any */
    char * (*nick)(struct irc * irc);  /* returns nick who sent this */
    char * (*real)(struct irc * irc);  /* returns realname part of nick who sent this */
    char * (*host)(struct irc * irc);  /* returns host part of nick who sent this */
    char * (*command)(struct irc * irc);  /* returns command name of this IRC message */
    short (*numeric)(struct irc * irc);  /* returns command name of this IRC message */
    int (*israw)(struct irc * irc);  /* Returns true if this command is a numeric and not a string command */
    unsigned long (*cid)(struct irc * irc); /* Returns the connection ID for this server to identify outgoing messages with */
    const char * (*server)(struct irc * irc); /* Returns the server name used to identify the server in multi-server commands */
    const char * (*setserver)(struct irc * irc, const char * server); /* Set the server name */

    void (*fdump)(struct irc * irc, FILE * fp); /* debugging, dumps contents to FILE ptr */

    /* Connect to a new server (add) */
    int (*connect)(struct irc * irc, const char * nick, const char * username, const char * host, unsigned int port, int ssl);  
    /* Reload perl scripts */
    int (*reload)(struct irc * irc);

    /* Actions/Output */
    int (*raw)(struct irc * irc, const char * msg); /* send raw server command */
    int (*privmsg)(struct irc * irc, const char * target, const char * msg); /* PRIVMSG command */
    int (*cprivmsg)(struct irc * irc, const char * target, const char * channel, const char * msg); /* CPRIVMSG command (directed, floodproof) */
    int (*notice)(struct irc * irc, const char * target, const char * msg);  /* NOTICE command */
    int (*say)(struct irc * irc, const char * msg);  /* Send PRIVMSG response back to target this came from */

    /* Actions/Channels */
    int (*join)(struct irc * irc, const char * target);  /* JOIN target */
    int (*joinkeys)(struct irc * irc, const char * target, const char * keys);  /* JOIN target */
    int (*part)(struct irc * irc, const char * target, const char * msg);  /* PART target, current channel (if there is one) if no target*/
    int (*topic)(struct irc * irc, const char * target, const char *msg);  /* TOPIC command */
    int (*mode)(struct irc * irc, const char * target, const char * flags, const char * args); /* MODE target flags [args] */
    int (*kick)(struct irc * irc, const char * target, const char * ktarget, const char * msg); /* MODE target flags [args] */

    /* Info lookup */
    int (*whois)(struct irc * irc, const char * target); /* WHOIS target */
    int (*who)(struct irc * irc, const char * mask); /* WHO mask */
    int (*userhost)(struct irc * irc, const char * users); /* USERHOST user user user ... */
};

#endif
