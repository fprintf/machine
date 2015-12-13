/*
 * Abstraction for modules
 *
 * - perl modules (mod_perl/)
 * - c modules (handled internally here)
 */

#include <stdbool.h>
#include <stdlib.h>      /* for exit() and etc.. */
#include <string.h>      /* for strerror() */

/* IPC/msg routines */
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h> /* getpid */
#include <errno.h>  /* errno for strerror() */

#include <event2/event.h> /* event_base etc.. */
#include <event2/bufferevent.h> /* bufferevent_* */
#include <event2/buffer.h>  /* evbuffer_readln */

#include <compat/queue.h> /* SLIST_* */

#include <vector.h>
#include <htable.h>
#include "xstr.h"
#include "irc.h"
#include "con.h"
#include "config.h"
#include "log.h"

/* language handlers */
#include "./mod_perl/mod_perl.h"

#define USE_PERL true

/* Worker process configuration */
#define MAX_WORKERS  4
#define MAX_REQUESTS 50

/* Server names can not be longer than 1024 letters */
#define MAX_SERVER_NAME 1024

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
    struct event * intsig; /* Handler for SIGINT signal */
    struct event * hupsig; /* Handler for SIGHUP signal */
    pid_t reapme;            /* Last reaped pid */
    SLIST_HEAD(workers, worker) * list;
} worker_list_initializer;
static struct worker_list * worker_list;


/* Forward declaration */
struct worker_list * workers_fork(struct event_base * evbase, register struct worker_list * wl, pid_t child);
static struct worker_list * workers_free(struct worker_list * wl);

static void worker_event_callback(struct bufferevent * bev, void * data)
{
    char * line;
    struct evbuffer * input = bufferevent_get_input(bev);
    
    while ((line = evbuffer_readln(input, NULL, EVBUFFER_EOL_CRLF))) {
//        log_debug("worker[%d] dispatching: %s", getpid(), line);
        irc.dispatch(line);
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

/* Forward declartion for parent_sig_callback below */
static int workers_command_reload(const char * argline);

static void parent_sig_callback(evutil_socket_t sig, short what, void * data)
{
    struct worker_list * list = data;
    pid_t child;
    int status;

    switch (sig) {
        case SIGINT:
            log_debug("parent [%d] caught SIGINT exiting...", getpid());
            event_base_loopexit(list->main_evbase, NULL);
            break;
        case SIGHUP:
            log_debug("parent [%d] caught SIGHUP reloading...", getpid());
			/* TODO RELOAD the config file then reload the children */
			workers_command_reload(NULL);
            break;
        case SIGCHLD:
            child = waitpid(0, &status, 0);
            log_debug("parent [%d] caught SIGCHLD for pid [%d]", getpid(), child);
            list->reapme = child;
            event_base_once(list->main_evbase, -1, EV_TIMEOUT, workers_spawn_callback, list, NULL);
            break;
        default:
            break;
    }
}


/* Sighandler for child processes */
static void worker_sig_callback(evutil_socket_t sig, short what, void * data)
{
    struct worker_ctx * ctx = data;

    switch (sig) {
        case SIGHUP:
            log_debug("child [%d] caught SIGINT exiting...", getpid());
            event_base_loopexit(ctx->base, NULL);
            break;
        default:
            break;
    }
}


/* Connect to a new server */
static int workers_command_connect(const char * argline)
{
    char host[BUFSIZ];
    char nick[BUFSIZ];
    char username[BUFSIZ];
    int port;
    int use_ssl;

    /* Connect to new server, %host %port %ssl */
    if (sscanf(argline, "%s %s %s %d %d", nick, username, host, &port, &use_ssl) == 5) {
        struct con * con = Con.new(host, port, CF_RECONNECT | (CF_SSL*use_ssl), worker_list);

        log_debug("[debug] attempting new connection: %s!%s -> %s://%s:%d",
                use_ssl ? "ircs" : "irc", nick, username, host, port);
        /* TODO need to set read and event callback to same as we set main connection */
        Con.callbacks(con, NULL, NULL, NULL);
        Con.connect(con, worker_list->main_evbase);
        /* Add server to vector at index CID */
        htable.store(gconfig.servers, "TODO", con);
    }

    return 0;
}
/* Restart children */
static int workers_command_reload(const char * argline)
{
    struct worker * worker;

    log_debug("[debug] reloading children...");

    SLIST_FOREACH(worker, worker_list->list, next) {
        if (worker->pid > 0) {
            log_debug("[debug] sending SIGHUP to %d", worker->pid);
            kill(worker->pid, SIGHUP);
        } else {
            log_debug("[debug] empty worker container? pid: %d", worker->pid);
        }
    }

    return 0;
}

static int workers_command(struct worker_list * wl, const char * command, const char * argline)
{
    static struct htable * command_lookup;
    int (*func)(const char * argline);
    
    if (!command_lookup) {
        command_lookup = htable.new(15); /* Make sure this is suitably larger than the number of commands */
        /* Register our commands (declared above) */
        htable.store(command_lookup, "CONNECT", workers_command_connect);
        htable.store(command_lookup, "RELOAD", workers_command_reload);
     /* TODO commands */
     /* TODO htable.store(command_lookup, "RECONNECT", workers_command_reconnect);  */
     /* TODO htable.store(command_lookup, "DISCONNECT", workers_command_disconnect);  */
    }

    if ( !(func = htable.lookup(command_lookup, command)) ) {
        log_debug("[debug] invalid command from child: %s", command);
        return -1;
    }

    /* Execute the command */
    return func(argline);
}


static void parent_event_callback(struct bufferevent * bev, void * data)
{
    char * line;
    struct evbuffer * input = bufferevent_get_input(bev);
    struct server * server;
	char server_name[MAX_SERVER_NAME];
    int consumed;

    while ((line = evbuffer_readln(input, NULL, EVBUFFER_EOL_CRLF))) {
        /* We got a management command */
        if (*line == ':') {
            char buf[BUFSIZ];

            /* Process commands */
            log_debug("parent received command from: %s", line);
            if (sscanf(line, ":%s %n", buf, &consumed)) 
                workers_command(data, buf, line + consumed);
        }
        /* else we got message to send to the server, output it */
        else if (sscanf(line, "S%s %n", server_name, &consumed)) {
            server = htable.lookup(gconfig.servers, server_name);

            if (server) {
                Con.puts(server->con, line + consumed);
            } else {
                log_debug("Trying to send to invalid server name: %s message: %s\n", server_name, line + consumed);
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
    kill(worker->pid, SIGHUP);
    waitpid(0, NULL, -1);

    /* Free resources */
    close(worker->sock);
    bufferevent_free(worker->bev);
    free(worker);
}

static void worker_init(int sock)
{
    struct worker_ctx ctx = {.base = NULL};

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

		/* Add sig handler */
		ctx.evsigint = evsignal_new(ctx.base, SIGHUP, worker_sig_callback, &ctx);

		sigset_t allsigs;
		sigfillset(&allsigs);
		sigprocmask(SIG_UNBLOCK, &allsigs, NULL);
        /* Begin main loop */
        event_base_dispatch(ctx.base);

        /* Cleanup */
        bufferevent_free(ctx.evsock);
        event_free(ctx.evsigint);
        event_base_free(ctx.base);
    } while(ctx.restart_loop);

    close(sock);
}

/*********************************************************
 * Workers container
 *********************************************************/
static struct worker_list * workers_free(struct worker_list * wl)
{
    if (!wl) 
        return NULL;

    while (!SLIST_EMPTY(wl->list)) {
        struct worker * worker = SLIST_FIRST(wl->list);
        SLIST_REMOVE_HEAD(wl->list, next);

        worker_free(worker);
    }
    event_free(wl->childsig);
    event_free(wl->intsig);
    event_free(wl->hupsig);
    free(wl->list);
    free(wl);
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
            printf("\n"); // TODO Flush stdout for some reason this is necessary?
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
		/* TODO fix this to use a hash table (keyed by pid) for the children instead of a list */
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
                        log_debug("reusing old worker[%d] container for new worker[%d]->bev[%p] with socket: %d", 
                                child, worker->pid, worker->bev, worker->sock);
                        break;
                    }
                }
                /* We found a replacement, use that struct */
                if (worker)
                    break;
                /* If we got here, we are probably in trouble.. */
                log_debug("WARNING we didn't find a container for pid: %d", child);
            }

            worker = malloc(sizeof *worker);
            if (!worker) {
                perror("workers_init malloc");
                goto error;
            }
            *worker = worker_initializer;
            SLIST_INSERT_HEAD(wl->list, worker, next);

            log_debug("inserted new worker [%p]", worker);
        } while (0);

        close(sockpair[0]); /* Close childs side of the socket */
        worker->pid = pid;
        worker->sock = sockpair[1]; 
        log_debug("worker launch pid [%d] socket fd[%d]", worker->pid, worker->sock);
    }

    return wl;

error:
    workers_free(wl);
    return NULL;
}

int mod_conf_init(void) {
	/* perl for global config reading */
	mod_perl_reinit();
	return 0;
}

int mod_conf_shutdown(void) {
	mod_perl_shutdown();
	return 0;
}

int mod_initialize(struct event_base * evbase)
{
    struct worker * worker;
    /* Init our config, prior to workers.. */
    /* TODO */
    /* Init our worker modules, each of wich has it's own perl interpreter  */
    worker_list = workers_fork(evbase, NULL, 0);
    /* Init the Listeners for each child on the parent side */
    /* Recreate any bufferevents if necessary */
    SLIST_FOREACH(worker, worker_list->list, next) {
        worker->bev = bufferevent_socket_new(evbase, worker->sock, 0);
        bufferevent_setcb(worker->bev, parent_event_callback, NULL, NULL, worker_list);
        bufferevent_enable(worker->bev, EV_READ);
        log_debug("worker[%d]->bev[%p] with socket: %d", 
                worker->pid, worker->bev, worker->sock);
    }
    /* Setup our sigchld catch */
    worker_list->childsig = evsignal_new(evbase, SIGCHLD, parent_sig_callback, worker_list);
    worker_list->intsig = evsignal_new(evbase, SIGINT, parent_sig_callback, worker_list);
    worker_list->hupsig = evsignal_new(evbase, SIGHUP, parent_sig_callback, worker_list);
    event_add(worker_list->childsig, NULL);
    event_add(worker_list->intsig, NULL);
    event_add(worker_list->hupsig, NULL);

    return 1;
}

void mod_shutdown(void)
{
    worker_list = workers_free(worker_list);
}

const char * mod_conf_get(const char * key)
{
    size_t len = strlen(key);
    return mod_perl_conf_get(key, len);
}

void mod_conf_servers(void (*cb)(struct server *)) {
	mod_perl_conf_servers(cb);
}

void mod_conf_foreach(const char * key, void (*cb)(const char * key, const char * val))
{
    size_t len = strlen(key);
    mod_perl_conf_foreach(key, len, cb);
}



int mod_dispatch(struct irc * event)
{
    mod_perl_dispatch(event);
    /* Release the memory associated with our event */
    irc.free(event);     
    return 0;
}

void mod_round_robin(struct server * server, const char * line)
{
    char * data;
    bool try_again;
    /* Send our event to the current child in our round-robin scheme */
    do {
        size_t linesize;
        try_again = false;
        worker_list->current = 
            worker_list->current ? worker_list->current : SLIST_FIRST(worker_list->list);

        xsprintf(&data, "S%s %s\n%zn", server->name, line, &linesize); 
//        log_debug("mod_round_robin dispatch: [%s]", data);
        if (send(worker_list->current->sock, data, linesize, 0) == -1) {
            perror("mod_round_robin send");
            try_again = true;
        }
        free(data);

        worker_list->current = SLIST_NEXT(worker_list->current, next);
    } while(try_again);
}
