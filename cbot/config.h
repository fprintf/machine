#ifndef CONFIG_HEADER__H_
#define CONFIG_HEADER__H_

#define MAX_SERVERS 1000

#include <stdbool.h>

#include <vector.h>

struct server {
    struct con * con; /* Connection object for this server */
    const char * name; /* Just a name to display for this server */
    const char * host;
    short int port;
    const char * pass;
    bool use_ssl;      /* We might want to convert these to bitwise flags if we get too many */
    bool use_password;
    bool use_knock;    /* Server requires a knock sequence to open the 'port' above! */
    const char * knock_sequence; /* This will contain a list of space separated ports to knock to open the port above */

	/* identity info */
    const char * nick; // nickname to use on the server
    const char * user;  // user/realname to use on the server;
};


struct config {
    const char * nick;
    const char * real;
    struct vector * servers;
    struct event_base * evbase;
};

extern struct config gconfig;
#endif
