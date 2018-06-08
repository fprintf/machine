#ifndef IRCMSG_TYPE__H_
#define IRCMSG_TYPE__H_

#define IRCMSG_MAXARGS 15

struct ircmsg_prefix {
    char * host;        /* user host or server name */
    char * nickname;    /* may be NULL if server message */
    char * realname;    /* may be NULL if server message */
};

static const struct ircmsg {
    /*
     * Identifies type of message
     * PRIVMSG, JOIN, etc..
     */
    enum ircmsg_type type;

	/* The name of the server this message came from (for lookup purposes */
	char servername[BUFSIZ]; 
	/* The server object from the config that this message came from */
	struct server * server;

    /*
     * Stores server or username
     * information depending
     * on who sent it
     */
    struct ircmsg_prefix * pfx;

    /*
     * If this is a raw numeric
     * IRC message, the numeric will
     * be stored here
     */
    short numeric;
    char * command; /* Stores command PRIVMSG, etc. */

    /*
     * "Trail" piece of IRC message, may be NULL
     * if there isn't any
     */
    char * text;  

    /* 
     * IRC message arguments, maximum 14 plus trailing
     * message which will be stored in 'text' above
     */
    char * argv[IRCMSG_MAXARGS];
    int argc;
} ircmsg_initializer;

#endif
