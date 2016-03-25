/*
 * Wrapper object around libevent handling for the
 * base socket connections for SSL/nonssl as well
 *
 * We can reconnect the con object by simply freeing
 * and recreating the buffer object (with some light 
 * munging for SSL objects) stored within, so the
 * con object does not have to be recreated.
 *
 */
#include <event2/dns.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/buffer.h> 
#include <event2/util.h>
#include <event2/event.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <stdarg.h>

#include "con.h"
#include "con.t"

/*
 * API LINKAGE AT BOTTOM OF FILE
 */

static void con_read_callback(struct bufferevent * bev, void * arg);
static void con_write_callback(struct bufferevent * bev, void * arg) { /*nothing here */; }
static void con_event_callback(struct bufferevent * bev, short events, void * arg);

static const struct con con_initializer = {
    .dnsbase = NULL,
    .bev = NULL,
    .host = NULL, 
    .port = 0, 
    .ssl     = NULL,
    .ssl_ctx = NULL, 
    .flags = CF_RECONNECT,
    .state = CS_DISCONNECTED,
    .cb = NULL,
    .userdata = NULL,
};

static unsigned int long con_id_track;

/*
 * Inits SSL framework and also creates/returns
 * a SSL_CTX object on request (only does initialization
 * once per run)
 */
SSL_CTX * con_ssl_init(void)
{
    static short con_ssl_init_done;
    SSL_CTX * ctx;

    /* Initialize the OpenSSL library */
    if (!con_ssl_init_done) {
        SSL_library_init();
        SSL_load_error_strings();
        ERR_load_crypto_strings();
        OpenSSL_add_all_algorithms();

        /* We MUST have entropy, or else there's no point to crypto. */
        if (!RAND_poll())
            return NULL;
        con_ssl_init_done = 1;
    }

    ctx = SSL_CTX_new(SSLv23_client_method());

    return ctx;
}

/*
 * Create new struct con object (allocated via malloc)
 *
 * Returns con object on success or NULL on failure
 */
struct con * con_new(const struct con * opt)
{
    struct con * nc = NULL;

    do {
        if (!(nc = malloc(sizeof *nc)))
            break;

        /*
         * Initialize to sane defaults
         */
        *nc = con_initializer;

        /* Allocate cb storage area */
        if (!(nc->cb = malloc(sizeof *nc->cb))) {
            free(nc); nc = NULL;
            break;
        }

        nc->id = con_id_track++;

        /* Copy callback pointers, if any */
        if (opt->cb) {
            nc->cb->readcb = opt->cb->readcb;
            nc->cb->writecb = opt->cb->writecb;
            nc->cb->eventcb = opt->cb->eventcb;
        }

        /* Copy only these (safe) items from opt */
        nc->host = opt->host;
        nc->port = opt->port;
        if (opt->ssl_ctx) nc->ssl_ctx = opt->ssl_ctx;
        if (opt->dnsbase) nc->dnsbase = opt->dnsbase;
        nc->userdata = opt->userdata;
        if (opt->flags) nc->flags = opt->flags;

    } while(0);

    return nc;
}

struct con * con_new_simple(const char * host, int port, short flags, void * userdata)
{
    struct con opt = {
        .host = host,
        .port = port,
        .flags = flags,
        .userdata = userdata
    };

    return con_new(&opt);
}


/*
 * Destroy con object, doesn't close any existing connection
 */
void con_free(struct con * con)
{
    if (con) {
        if (con->ssl) {
            /* need this for the connection to close cleanly
             * when using BUFFEREVENT_OPT_CLOSE_ON_FREE */
            SSL_set_shutdown(con->ssl, SSL_RECEIVED_SHUTDOWN);
            SSL_shutdown(con->ssl);
            con->ssl = NULL;
        }
        bufferevent_free(con->bev); /* frees ssl object */
        con->bev = NULL;
		//event_free(con->tev);

        /* Free (possibly) shared objects */
        evdns_base_free(con->dnsbase, 1); con->dnsbase = NULL;
        SSL_CTX_free(con->ssl_ctx); con->ssl_ctx = NULL;
        free(con->cb); /* free callback storage */
        free(con);
    }
}

/*
 * Fires off the connection request for a struct con
 * object. Requires passing a libevent base and dnsbase
 * to use for resolving hostnames
 *
 * Returns 0 for failure (should check error stack TODO) or 1 for succes
 */
int con_connect(struct con * con, struct event_base * evbase)
{
    struct bufferevent * bev;
    int success = 0;

    do {
        /* SSL enabled */
        if (con->flags & CF_SSL) {
            /* Init SSL and get our SSL_CTX (TODO check error stack) */
            if (!con->ssl_ctx)
                if ( !(con->ssl_ctx = con_ssl_init()) )
                    break;
            /* create SSL if we don't have one */
            if (!con->ssl) con->ssl = SSL_new(con->ssl_ctx);
            if (!con->ssl)
                break;

            bev = bufferevent_openssl_socket_new(evbase, 
                    -1,
                    con->ssl, 
                    BUFFEREVENT_SSL_CONNECTING, 
                    BEV_OPT_CLOSE_ON_FREE
                    );
        /* NON SSL */
        } else  {
            bev = bufferevent_socket_new(evbase, -1, BEV_OPT_CLOSE_ON_FREE);
        }

        /* TODO: check error stack here */
        if (!bev) 
            break;
        con->bev = bev; /* mainly so we can free it on close */

        /* Setup DNS base */
        if (!con->dnsbase) 
            con->dnsbase = evdns_base_new(evbase, 1);

        /* Establish callbacks (make con object the argument) */
        bufferevent_setcb(bev, con_read_callback, con_write_callback, con_event_callback, con);
        bufferevent_enable(bev, EV_READ|EV_WRITE);
        /* Set the read timeout on the socket so if we stop getting data for this long, restart */
        bufferevent_set_timeouts(bev, &(struct timeval){.tv_sec = CON_READ_TIMEOUT}, NULL);

        /* Actually fire off the connect request. We need to actually have
         * an event base dispatched before anything happens here.. */
        bufferevent_socket_connect_hostname(bev, con->dnsbase, AF_UNSPEC, con->host, con->port);

        /* Done */
        success = 1;
    } while(0);

    return success;
}

/*
 * Reconnect the con object (without destroying it)
 *
 * Returns 0 on failure and 1 for sucess
 *
 */
int con_reconnect(struct con * con)
{
    struct event_base * evbase;
    int r = 0;

    if (con) {
        if (con->flags & CF_SSL) {
            SSL_set_shutdown(con->ssl, SSL_RECEIVED_SHUTDOWN);
            SSL_shutdown(con->ssl);
            con->ssl = NULL;
        }
        if (con->bev) {
            evbase = bufferevent_get_base(con->bev);
            bufferevent_free(con->bev); /* also frees SSL object */
            con->bev = NULL;
        }

        /* Now our ssl and bufferevent objects will be recreated */
        r = con_connect(con, evbase);
    }

    return r;
}



/*
 * Write formatted message to con object
 *
 * Return: See evbuffer_add_printf for return value
 */
int con_printf(struct con * con, const char * fmt, ...)
{
    va_list ap;
    struct evbuffer * out;
    int r = 0;

    if (con) {
        out = bufferevent_get_output(con->bev);

        va_start(ap, fmt);
        r = evbuffer_add_vprintf(out, fmt, ap);
        va_end(ap);
    }

    return r;
}

/*
 * Write unformatted message to a con object
 * automatically appends CRLF
 *
 * Return: See evbuffer_add_printf for return value
 */
int con_puts(struct con * con, const char * s)
{
    struct evbuffer * out;
    int r = 0;

    if (con) {
        out = bufferevent_get_output(con->bev);

        /* Append \r\n automatically */
        r = evbuffer_add_printf(out, "%s\r\n", s);
    }

    return r;
}

/*
 * Setters/getters for opaque interface
 */
int con_flag_ssl(struct con * con, short tf)
{
    return tf ? (con->flags |= CF_SSL) : con->flags & CF_SSL;
}

int con_flag_reconnect(struct con * con, short tf)
{
    return tf ? (con->flags |= CF_RECONNECT) : con->flags & CF_RECONNECT;
}

void con_callbacks(struct con * con, void * readcb, void * writecb, void * eventcb)
{
    if (readcb) con->cb->readcb = readcb;
    if (writecb) con->cb->writecb = writecb;
    if (eventcb) con->cb->eventcb = eventcb;
}

/* Set to NULL host if you just want to set port or 0 for port if just host */
int con_port(struct con * con, int port)
{
    return port ? (con->port = port) : con->port;
}
const char * con_host(struct con * con, const char * host)
{
    return host ? (con->host = host) : con->host;
}
void * con_userdata(struct con * con, void * userdata)
{
    return userdata ? (con->userdata = userdata) : con->userdata;
}
unsigned long con_cid(struct con * con)
{
    return con->id;
}


/* Init our exported API table */
const struct con_api Con = {
    .new = con_new_simple,
    .copy = con_new,
    .free = con_free,
    .connect = con_connect,

    /* Set/Getters */
    .fssl = con_flag_ssl, 
    .freconnect = con_flag_reconnect,
    .host = con_host, 
    .port = con_port,
    .cid = con_cid,
    .userdata = con_userdata,
    .callbacks = con_callbacks,

    /* Write interface */
    .printf = con_printf,
    .puts = con_puts,
};




/**************************************************************************
 * CALLBACK HANDLERS, THESE MASK THE LIBEVENT HANDLERS READ/WRITE/EVENT
 * In each one we will do some base work, then call the users callback
 * if he passed us one (supplying it the 'userdata' object in the struct)
 **************************************************************************/

int con_dispatch_readln(struct con * con, const char * s, void * userdata)
{
    int ret = 0;
    int (*readcb)(struct con * con, const char * s, void * userdata) = 
        con->cb->readcb;

    if (readcb)
        ret = readcb(con, s, userdata);

    return ret;
}

int con_dispatch_write(struct con * con, const char * s, void * userdata)
{
    int ret = 0;
    int (*writecb)(struct con * con, const char * s, void * userdata) = 
        con->cb->writecb;

    if (writecb)
        ret = writecb(con, s, userdata);

    return ret;
}

int con_dispatch_event(struct con * con, short event, void * userdata)
{
    int ret = 0;
    int (*eventcb)(struct con * con, short event, void * userdata) = 
        con->cb->eventcb;

    if (eventcb)
        ret = eventcb(con, event, userdata);

    return ret;
}

/* 
 * libevent read callback handler
 */
static void con_read_callback(struct bufferevent * bev, void * arg)
{
    struct con * con = arg;
    char * line;
    struct evbuffer * input = bufferevent_get_input(bev);
    int cont;

    while ((line = evbuffer_readln(input, NULL, EVBUFFER_EOL_CRLF)) != 0) {
        cont = con_dispatch_readln(con, line, con->userdata);
        free(line); /* cleanup */
        /* We are leaving input in the buffer..(at request of user) */
        if (!cont)
            break;
    }
}

/* 
 * libevent 'event' callback handler 
 */
static void con_event_callback(struct bufferevent *bev, short events, void * arg)
{
    struct con * con = arg;
    enum con_events type = 
        (events & BEV_EVENT_CONNECTED ? 1 : 0) +
        (events & (BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT) ? (1 << 1) : 0) +
        (events & BEV_EVENT_EOF ? (1 << 2) : 0);

    /* CONNECT */
    switch(type) {
        case CON_EVENT_CONNECTED: ; 
            /* Clear reconnecting flag, if we were reconnecting.. */
            con->state &= ~CS_RECONNECTING;
            /* Clear disconnected flag, as we're not disconnected anymore :) */
            con->state &= ~CS_DISCONNECTED;

            /* Call our usercallback, if any */
            con_dispatch_event(con, CON_EVENT_CONNECTED, con->userdata);
            break; 

        case CON_EVENT_ERROR: ;
            int err;
            /* 
             * Check various types of errors..
             */

            /* DNS error, if so we can't continue */
            if ( (err = bufferevent_socket_get_dns_error(bev)) )
                printf("DNS error: %s\n", evutil_gai_strerror(err));
            else 
                printf("Error from %s: %s\n",
                        con->host, evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
            /* Check for ssl errors */
            {
                unsigned long err;
                while ((err = (bufferevent_get_openssl_error(bev)))) {
                    const char *msg = ERR_reason_error_string(err);
                    const char *lib = ERR_lib_error_string(err);
                    const char *func = ERR_func_error_string(err);
                    fprintf(stderr, "%s in %s %s\n", msg, lib, func);
                }
                if (errno)
                    perror("connection error");
            }
            /* Call error event */
            con_dispatch_event(con, CON_EVENT_ERROR, con->userdata);


            /* We weren't able to restore the connect, clean up */
            fprintf(stderr, "Connection closed due to error\n");
// We're not freeing because we want to fall through below and reconnect
//            bufferevent_free(con->bev);
//            con->bev = NULL; /* MUST SET TO NULL, TO AVOID DUPLICATE FREE */
//            con->ssl = NULL; /* MUST set to NULL, will be invalid now (see con_free) */

			/* FALL THROUGH TO TRY AND RECONNECT */
			/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

        case CON_EVENT_EOF:
            con->state |= CS_DISCONNECTED;

            con_dispatch_event(con, CON_EVENT_EOF, con->userdata);
            /* Connection closed, should we attempt to reconnect? */
            if (con->flags & CF_RECONNECT && !con_reconnect(con)) 
                fprintf(stderr, "Failed to reconnect, closing.\n");
            break;


        default:
            fprintf(stderr, "INVALID EVENT, SHOULD NEVER HAPPEN, WHAT HAVE YOU DONE?!?\n");
            break;
    }

    /* end */
}
