#ifndef CON_HEADER__H_
#define CON_HEADER__H_

#include <event2/event.h>

/* Flags */
enum con_flags {
    CF_NONE        = 1 << 0, /* default flags */
    CF_SSL         = 1 << 1, /* negotiate SSL for this connection */
    CF_RECONNECT   = 1 << 2, /* automatically reconnect when disconnected */
};

enum con_events {
    CON_EVENT_UNSET,
    CON_EVENT_CONNECTED = 1 << 0,
    CON_EVENT_ERROR     = 1 << 1,
    CON_EVENT_EOF       = 1 << 2,
};

/* Interface */
extern const struct con_api Con;

struct con_api {
    /* Create/destroy/connect connections */
    struct con * (*new)(const char * host, int port, short flags, void * userdata);
    struct con * (*copy)(const struct con * opt);
    void (*free)(struct con * con); /* Also disconnects */
    int (*connect)(struct con * con, struct event_base * evbase);

    /* Set/Get */
    /*
     * Generally these functions return the current value if you provide
     * a "false" value for the parameter 
     */
    int (*fssl)(struct con * con, short tf); 
    int (*freconnect)(struct con * con, short tf);
    /* Set port to 0 to return current port, or host to NULL to return current host */
    int (*port)(struct con * con, int port);
    const char * (*host)(struct con * con, const char * host);
    unsigned long (*cid)(struct con * con);
    void * (*userdata)(struct con * con, void * userdata);
    void (*callbacks)(struct con * con, void * readcb, void * writecb, void * eventcb);

    /* Write interface to connection */
    int (*printf)(struct con * con, const char * fmt, ...);
    int (*puts)(struct con * con, const char * s);
};


#endif
