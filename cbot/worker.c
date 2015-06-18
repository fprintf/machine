/*
 * Pre-forking manager
 */

#include <stdbool.h>
#include <stdlib.h>      /* for exit() and etc.. */
#include <string.h>      /* for strerror() */

#include <sys/types.h>

#include <unistd.h> /* getpid */
#include <errno.h>  /* errno for strerror() */

#include <event2/event.h> /* event_base etc.. */
#include <event2/bufferevent.h> /* bufferevent_* */
#include <event2/buffer.h>  /* evbuffer_readln */

#include <compat/queue.h> /* SLIST_* */
#include "xstr.h"
#include "irc.h"
#include "config.h"
#include "log.h"

#define USE_PERL true

/* Worker process configuration */
#define MAX_WORKERS  4
#define MAX_REQUESTS 50

/* Data used by master to keep track of worker process */
static const struct worker {
    pid_t pid;
    int sock; /* Communication channel to child */
    struct bufferevent * bev;
    SLIST_ENTRY(worker) next;
} worker_initializer;

/* Data used by worker process */
struct worker_ctx {
    struct event_base * base;
    struct event * evsigint, * evsighup;
    struct bufferevent * evsock;
    bool restart_loop;
};

/* Master list of workers */
static const struct worker_list {
    struct event_base * main_evbase; /* Evbase we need to free in our children */
    size_t active_workers;
    struct worker * current; /* For round robin */
    struct event * childsig; /* Handler for SIGCHLD signal */
    struct event * intsig;   /* Handler for SIGINT signal */
    struct event * hupsig;   /* Handler for SIGHUP signal */
    pid_t reapme;            /* Last reaped pid */
    SLIST_HEAD(workers, worker) * list;
} worker_list_initializer;
static struct worker_list * worker_list;


/* Forward declaration */
struct worker_list * workers_fork(struct event_base * evbase, register struct worker_list * wl, pid_t child);

/*********************************************************************
 * Worker callbacks (code executed in the child)
 *********************************************************************/
static void worker_sig_callback(evutil_socket_t sig, short what, void * data)
{
    struct worker_ctx * ctx = data;

    switch (sig) {
        case SIGINT:
            log_debug("child [%d] caught SIGINT exiting...", getpid());
            event_base_loopexit(ctx->base, NULL);
            break;
        case SIGHUP:
            ctx->restart_loop = true; /* Restart our child */
            event_base_loopexit(ctx->base, NULL);
            break;
#if NDEBUG
        default:
            log_debug("child [%d] uncaught signal %d", getpid(), sig);
            break;
#endif
    }
}

static void worker_event_callback(struct bufferevent * bev, void * data)
{
    char * line;
    struct evbuffer * input = bufferevent_get_input(bev);
    
    while ((line = evbuffer_readln(input, NULL, EVBUFFER_EOL_LF))) {
        log_debug("pid [%d] received msg: %s", getpid(), line);
        irc.dispatch(line, NULL);
        free(line);
    }
}

static void parent_event_callback(struct bufferevent * bev, void * data);

static void workers_spawn_callback(evutil_socket_t sock, short what, void * data)
{
    struct worker_list * list = data;
    /* Now fork a new child */
    workers_fork(list->main_evbase, list, list->reapme);
    list->reapme = 0;
}

static void parent_sig_callback(evutil_socket_t sig, short what, void * data)
{
    struct worker_list * list;
    pid_t child;

    switch (sig) {
        case SIGCHLD:
            child = wait(NULL);
            log_debug("parent [%d] caught SIGCHLD for pid [%d]", getpid(), child);

            list = data;
            list->reapme = child;
            event_base_once(list->main_evbase, -1, EV_TIMEOUT, workers_spawn_callback, data, NULL);
            break;
        case SIGINT:
            log_debug("child [%d] caught SIGINT exiting...", getpid());
            event_base_loopexit(list->main_evbase, NULL);
            break;
        case SIGHUP:
            log_debug("parent [%d] caught SIGHUP ..should reload...", getpid());
            break;
        default:
            break;
    }
}

static int workers_command(struct worker_list * wl, const char * command, const char * argline)
{
    /* TODO lookup command in hashtable */
    if (strcmp(command, "CONNECT") == 0) {
        char host[BUFSIZ];
        int port;
        int use_ssl;

        /* Connect to new server, %host %port %ssl */
        if (sscanf(argline, "%s %d %d", host, &port, &use_ssl) == 3) {
            struct con * con = Con.new(host, port, CF_RECONNECT | (CF_SSL*use_ssl), wl);

            log_debug("[debug] attempting new connection: %s://%s:%d",
                    use_ssl ? "ircs" : "irc", host, port);
            /* TODO need to set read and event callback to same as we set main connection */
            Con.callbacks(con, NULL, NULL, NULL);
            Con.connect(con, wl->main_evbase);
        }
    }
    else if (strcmp(command, "RELOAD") == 0) {
        /* Restart children */
    }

    return 0;
}


static void parent_event_callback(struct bufferevent * bev, void * data)
{
    char * line;
    struct evbuffer * input = bufferevent_get_input(bev);
    struct con * con;
    unsigned long cid;
    int consumed;
    
    while ((line = evbuffer_readln(input, NULL, EVBUFFER_EOL_CRLF))) {
        /* We got a management command */
        if (*line == ':') {
            char buf[BUFSIZ];

            /* Process commands */
            log_debug("[debug] parent received command from: %s", line);
            if (sscanf(line, ":%s %n", buf, &consumed)) 
                workers_command(data, buf, line + consumed);
        }
        /* else we got message to send to the server, output it */
        else if (sscanf(line, "S%lu %n", &cid, &consumed)) {
            log_debug("vector: %p cidindex: %p parent read line [%s] to CID: [%lu]", gconfig.servers, vector.index(gconfig.servers, cid), line + consumed, cid);
            con = vector.index(gconfig.servers, cid);
            if (con) {
                log_debug("parent sending [%s] to CID: [%lu]", line + consumed, cid);
                Con.puts(con, line + consumed);
            }
        }

        free(line);
    }
}

/*********************************************************************
 * Worker element interface
 *********************************************************************/
static void worker_free(struct worker * worker)
{
    if (!worker) 
        return;
    /* Kill and wait for pid */
    kill(worker->pid, SIGINT);
    waitpid(worker->pid, NULL, -1);

    /* Free resources */
    close(worker->sock);
    bufferevent_free(worker->bev);
    free(worker);
}

static void worker_init(int sock)
{
    struct worker_ctx ctx = {0};

    do {
        ctx.restart_loop = 0;
        /* Init perl in our child */
        mod_perl_reinit(); 

        ctx.base = event_base_new();
        if (!ctx.base) {
            perror("child base");
            break;
        }

        ctx.evsock = bufferevent_socket_new(ctx.base, sock, 0);
        bufferevent_setcb(ctx.evsock, worker_event_callback, NULL, NULL, ctx.evsock);
        bufferevent_enable(ctx.evsock, EV_READ);
        event_add(ctx.evsigint, NULL);
        event_add(ctx.evsighup, NULL);

        /* Begin main loop */
        event_base_dispatch(ctx.base);

        /* Cleanup */
        event_free(ctx.evsigint);
        event_free(ctx.evsighup);
        bufferevent_free(ctx.evsock);
        event_base_free(ctx.base);
    } while(ctx.restart_loop);

    close(sock);
}

/*********************************************************
 * Workers container
 *********************************************************/
static struct worker_list * workers_free(struct worker_list * worker_list)
{
    if (!worker_list) 
        return NULL;

    while (!SLIST_EMPTY(worker_list->list)) {
        struct worker * worker = SLIST_FIRST(worker_list->list);
        SLIST_REMOVE_HEAD(worker_list->list, next);

        worker_free(worker);
    }
    event_free(worker_list->childsig);
    event_free(worker_list->intsig);
    event_free(worker_list->hupsig);
    free(worker_list->list);
    free(worker_list);
    return NULL;
}

struct worker_list * workers_fork(struct event_base * evbase, register struct worker_list * wl, pid_t child)
{
    struct worker * worker;

    if (!wl) {
        if (!(wl = malloc(sizeof *wl))) {
            perror("workers_init malloc");
            goto error;
        }

        /* Init our data struct */
        *wl = worker_list_initializer;
        wl->main_evbase = evbase;
        if (!(wl->list = malloc(sizeof *wl->list))) {
            perror("workers_init malloc");
            goto error;
        }
        SLIST_INIT(wl->list);
    }

    /* We got called from SIGCHLD we need to fork a new child */
    if (child > 0) 
        --wl->active_workers;

    for (; wl->active_workers < MAX_WORKERS; ++wl->active_workers) {
        int sockpair[2];
        pid_t pid;

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == -1) {
            perror("workers_init socketpair");
            goto error;
        }

        /* Fork */
        pid = fork();
        if (pid == 0) {
            /******************
             * Child process *
             ******************/
            /* Need to free our primary eventbase in the child
             * every time we launch a new child */
            if (wl->main_evbase) {
                /* we MUST reinit before freeing or else everything stops working */
                event_reinit(wl->main_evbase);
                event_base_free(wl->main_evbase); 
            }
            /* Close parents end of the socket */
            close(sockpair[1]);
            /* Dup stdout in our child to the other end of our socket */
            if (dup2(sockpair[0], STDOUT_FILENO) == -1) {
                perror("workers_init dup2");
                goto error;
            }

            /* Call our child function */
            worker_init(sockpair[0]);

            exit(0);
        } else if (pid == -1) {
            perror("worker_init fork");
            goto error;
        }

        /******************
         * Parent process *
         ******************/
        /* We need to reuse existing if we lost a child */
        worker = NULL;
        do {
            if (child > 0) {
                SLIST_FOREACH(worker, wl->list, next) {
                    if (worker->pid == child) {

                        /* Free our old bufferevent and get a new one for the new socket */
                        bufferevent_free(worker->bev);
                        close(worker->sock);
                        worker->bev = bufferevent_socket_new(wl->main_evbase, sockpair[1], 0);
                        bufferevent_setcb(worker->bev, parent_event_callback, NULL, NULL, wl);
                        bufferevent_enable(worker->bev, EV_READ);
                        log_debug("reusing [old %d] worker [%p] new bev [%p] for worker->pid [%d] with socket: %d", 
                                child, worker, worker->bev, worker->pid, worker->sock);
                        break;
                    }
                }
                /* We found a replacement, use that struct */
                if (worker)
                    break;
            }

            worker = malloc(sizeof *worker);
            if (!worker) {
                perror("workers_init malloc");
                goto error;
            }
            *worker = worker_initializer;

            log_debug("allocated new worker [%p]", worker);
        } while (0);

        close(sockpair[0]); /* Close childs side of the socket */
        worker->pid = pid;
        worker->sock = sockpair[1]; 
        log_debug("pid [%d] socket fd[%d]", worker->pid, worker->sock);
        SLIST_INSERT_HEAD(wl->list, worker, next);
    }


    return wl;

error:
    workers_free(wl);
    return NULL;
}

int mod_initialize(struct event_base * evbase)
{
    struct worker * worker;
    /* TODO Init our config, prior to workers.. */

    /* Init our worker modules, each of wich has it's own perl interpreter  */
    worker_list = workers_fork(evbase, NULL, 0);
    /* Init the Listeners for each child on the parent side */
    /* Recreate any bufferevents if necessary */
    SLIST_FOREACH(worker, worker_list->list, next) {
        worker->bev = bufferevent_socket_new(worker_list->main_evbase, worker->sock, 0);
        bufferevent_setcb(worker->bev, parent_event_callback, NULL, NULL, worker->bev);
        bufferevent_enable(worker->bev, EV_READ);
        log_debug("new bev [%p] for worker->pid [%d] with socket: %d", 
                worker->bev, worker->pid, worker->sock);
    }
    /* Setup our sigchld catch */
    worker_list->childsig = evsignal_new(evbase, SIGCHLD, parent_sig_callback, worker_list);
    worker_list->evsigint = evsignal_new(evbase, SIGINT, parent_sig_callback, worker_list);
    worker_list->evsighup = evsignal_new(evbase, SIGHUP, parent_sig_callback, worker_list);
    event_add(worker_list->childsig, NULL);
    event_add(worker_list->intsig, NULL);
    event_add(worker_list->hupsig, NULL);

    return 1;
}

void mod_shutdown(void)
{
    worker_list = workers_free(worker_list);
}

int mod_dispatch(struct irc * event)
{
    mod_perl_dispatch(event);
    /* Release the memory associated with our event */
    irc.free(event);     
    return 0;
}

void mod_round_robin(struct con * con, const char * line)
{
    char * data;
    bool try_again;
    /* Send our event to the current child in our round-robin scheme */
    do {
        size_t linesize;
        try_again = false;
        worker_list->current = 
            worker_list->current ? worker_list->current : SLIST_FIRST(worker_list->list);

        xsprintf(&data, "S%d %s\n%zn", Con.cid(con), line, &linesize); 
        log_debug("sending: [%s]", data);
        if (send(worker_list->current->sock, data, linesize, 0) == -1) {
            perror("mod_round_robin send");
            try_again = true;
        }
        free(data);

        worker_list->current = SLIST_NEXT(worker_list->current, next);
    } while(try_again);
}
