#ifndef CONFIG_HEADER__H_
#define CONFIG_HEADER__H_

#define MAX_SERVERS 1000

#include <stdbool.h>

#include <vector.h>

struct server {
    struct con * con; /* Connection object for this server */
    const char * name; /* Just a name to display for this server */
    const char * pass;
    const char * nickname;  /* nickname for this server */
    const char * username;  /* Username for this server */
    bool use_knock;    /* Server requires a knock sequence to open the 'port' above! */
    const char * knock_sequence; /* This will contain a list of space separated ports to knock to open the port above */
};


struct config {
    const char * nick;
    const char * real;
    struct vector * servers;
    struct event_base * evbase;
};

/* Build a server */
struct server * make_server(const char * name);
/* Destroy a server */
void destroy_server(struct server ** server);


struct keydata {
    enum { KEYDATA_STRING, KEYDATA_INDEX } type;
    const char * key;
    const char * parentkey;
    int32_t index;
};


extern struct config gconfig;
#endif
