#ifndef CON_HEADER_TYPE__H_
#define CON_HEADER_TYPE__H_

enum con_state {
    CS_DISCONNECTED = 1 << 0,
    CS_RECONNECTING = 1 << 1,
};

struct con_cb {
    void * readcb;
    void * writecb;
    void * eventcb;
};

struct con {
    unsigned long id;
    struct bufferevent * bev;
    struct evdns_base * dnsbase;
    const char * host;
    int port;

    SSL * ssl;
    SSL_CTX * ssl_ctx;

    void * userdata;

    enum con_flags flags;
    enum con_state state;

    struct con_cb * cb; /* User callback linkage */
};

#endif
