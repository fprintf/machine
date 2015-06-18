/* ircmsg.c IRC MESSAGE parsing routines */

#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strncmp */

#include <htable.h> /* Urlib */
#include "xstr.h" /* xstrndup */

#include "ircmsg.h"
#include "ircmsg.t"

#define ARRAY_SIZE(x) (sizeof (x) / sizeof *(x))

static enum ircmsg_type ircmsg_determine_type(struct ircmsg * msg)
{
    struct types_ {
        const char * s;
        enum ircmsg_type type;
    };
    static struct htable * htab;

    if (!htab) {
        size_t i;
        static struct types_ types[] = { 
            {"JOIN", IRC_JOIN},
            {"KICK", IRC_KICK},
            {"MODE", IRC_MODE},
            {"NICK", IRC_NICK},
            {"NOTICE", IRC_NOTICE},
            {"PART", IRC_PART},
            {"PING", IRC_PING},
            {"PRIVMSG", IRC_PRIVMSG},
            {"QUIT", IRC_QUIT},
            {"TOPIC", IRC_TOPIC},
        };

        htab = htable.new(30);

        for (i = 0; i < sizeof types / sizeof *types; ++i)
            htable.store(htab, types[i].s, &types[i]);
    }

    /* Check for NUMERIC type first as it's the common case, then command */
    if (*msg->command >= '0' && *msg->command <= '9') {
        msg->type = IRC_NUMERIC;
        sscanf(msg->command, "%03hd", &msg->numeric);
    } else {
        /* Check hash table for our type */
        struct types_ * t = htable.lookup(htab, msg->command);

        if (t)
            msg->type = t->type;
    }


    return msg->type;
}

/****************************************************************************
 * FSM parser for IRC Message format
 ****************************************************************************/
struct ircmsg_state {
    const char * line;
    struct ircmsg * event;

    /* Set to next state and return function, NULL stops FSM */
    struct ircmsg_state * (*next)(struct ircmsg_state * state);
};
static struct ircmsg_state * ircm_st_cid(struct ircmsg_state * state);
static struct ircmsg_state * ircm_st_begin(struct ircmsg_state * state);
static struct ircmsg_state * ircm_st_prefix(struct ircmsg_state * state);
static struct ircmsg_state * ircm_st_command(struct ircmsg_state * state);
static struct ircmsg_state * ircm_st_params(struct ircmsg_state * state);

static struct ircmsg_state * ircm_st_cid(struct ircmsg_state * state)
{
    int consumed;
    state->next = ircm_st_begin;

    /* Parse the connection id */
    if (sscanf(state->line, "S%lu %n", &state->event->cid, &consumed))
        state->line += consumed;
    return state;
}

static struct ircmsg_state * ircm_st_begin(struct ircmsg_state * state)
{
    /* Check for prefix (and skip ':'), or move on to command state */
    if (*state->line == ':' && ++state->line)
        state->next = ircm_st_prefix;
    else
        state->next = ircm_st_command;
    return state;
}

static struct ircmsg_state * ircm_st_prefix(struct ircmsg_state * state)
{
    const char * p;
    state->next = ircm_st_command;
    char cont = 1;

    for (p = state->line; *p && cont; ++p) {
        enum { BANG=1<<0, ATSIGN=1<<1, SPACE=1<<2 };
        register short is_tok = (
                  (*p == '!' ? BANG : 0)
                | (*p == '@' ? ATSIGN : 0)
                | (*p == ' ' ? SPACE : 0) 
        );
        /* Outer 'if' used to group state->line = p+1 assignment
         * which has to come after the switch.. and would
         * otherwise be repeated in each case */
        if (is_tok) {
            switch(is_tok) {
            case BANG:
                state->event->pfx->nickname = xstrndup(state->line, p - state->line );
                break;
            case ATSIGN:
                state->event->pfx->realname = xstrndup(state->line, p - state->line );
                break;
            case SPACE:
                state->event->pfx->host = xstrndup(state->line, p - state->line );
                cont = 0; /* Stop looping here.. */
                break;
            }
            /* start at next point (for copy) */
            state->line = ++p;
        }
    }

    return state;
}

static struct ircmsg_state * ircm_st_command(struct ircmsg_state * state)
{
    const char * p;
    state->next = ircm_st_params;

    if ( (p = xstrchrnul(state->line, ' ')) ) {
        state->event->command = xstrndup(state->line, p - state->line );
        state->line = p;
        /* Now check the type we just copied into ->command */
        ircmsg_determine_type(state->event);
        /* Skip over the space we just found now */
        if (*state->line) ++state->line;
    }

    return state;
}

static struct ircmsg_state * ircm_st_params(struct ircmsg_state * state)
{
    const char * p;

    /* We always end here */
    state->next = NULL;

    for (p = state->line; *p; ++p) {
        /* Found a text parameter, rest of string is our text */
        if (*state->line == ':') {
            state->event->text = xstrdup(++state->line);
            break; /* we're done */
        }

        /* Parameter, copy it in and continue */
        if (*p == ' ') {
            if (state->event->argc >= ARRAY_SIZE(state->event->argv)) 
                break;
            state->event->argv[state->event->argc++] = xstrndup(state->line, p - state->line );
            state->line = p+1;
        }
    }

    return state;
}

/*
 * RUN the FSM
 */
static void ircm_run(struct ircmsg_state * state)
{
    /* 
     * Begin running 
     */
    while (state->next != NULL) {
        /* Run current state (pass self as object) */
        state->next(state);


        /* Parse Failure!
         * We could check here for the current state
         * and report some more data..
         */
        if (!*state->line) 
            state->next = NULL;
    }
}
/****************************************************************************
 * END FSM CODE
 ****************************************************************************/

static struct ircmsg * ircmsg_parse(const char * line)
{
    struct ircmsg * msg;

    do {
        struct ircmsg_state state;

        msg = malloc(sizeof *msg);
        if (!msg)
            break;
        *msg = ircmsg_initializer;

        msg->pfx = malloc(sizeof *msg->pfx);
        if (!msg->pfx) {
            free(msg);
            msg = NULL;
            break;
        }
        msg->pfx->host = 
            msg->pfx->nickname =
            msg->pfx->realname =
            NULL;

        /* Begin parsing */
        state.event = msg;
        state.line = line;
        state.next = ircm_st_cid;
        ircm_run(&state); /* start the EFSM */
    }while(0);

    return msg;
}


static void ircmsg_free(struct ircmsg * msg)
{
    if (msg) {
        if (msg->pfx) {
            free(msg->pfx->nickname);
            free(msg->pfx->realname);
            free(msg->pfx->host);
            free(msg->pfx); msg->pfx = NULL;
        }
        free(msg->text);
        free(msg->command);

        for (--msg->argc; msg->argc >= 0; --msg->argc)
            free(msg->argv[msg->argc]);
    }
}

void ircmsg_print(struct ircmsg * msg, FILE * fp)
{
    if (msg) {
        int argc;

        if (msg->pfx) {
            if (msg->pfx->nickname) 
                fprintf(fp, "<%s!%s@%s>", msg->pfx->nickname, msg->pfx->realname, msg->pfx->host);
            else
                fprintf(fp, "h:%s", msg->pfx->host);
        }

        fprintf(fp, " c:%s ", msg->command);

        for (argc = 0; argc < msg->argc; ++argc)
            fprintf(fp, "p:%s ", msg->argv[argc]);

        fprintf(fp, "t:%s", msg->text);

        /* end */
        fprintf(fp, "\n");
    }
}

/*
 * Getters
 */
enum ircmsg_type ircmsg_get_type(struct ircmsg * msg) { return msg->type; }
char * ircmsg_get_command(struct ircmsg * msg) { return msg->command; }
char * ircmsg_get_text(struct ircmsg * msg) { return msg->text; }
short ircmsg_get_numeric(struct ircmsg * msg) { return msg->numeric; }
char * ircmsg_get_nick(struct ircmsg * msg) { return msg->pfx->nickname; }
char * ircmsg_get_real(struct ircmsg * msg) { return msg->pfx->realname; }
char * ircmsg_get_host(struct ircmsg * msg) { return msg->pfx->host; }
unsigned long ircmsg_get_cid(struct ircmsg * msg) { return msg->cid; }
/* Get parameter at index 'argc' */
char * ircmsg_get_argv(struct ircmsg * msg, int argc) { if (argc >= msg->argc) return NULL; return msg->argv[argc]; }


const struct ircmsg_api Msg = {
    .parse = ircmsg_parse,
    .free = ircmsg_free,

    /* Getters */
    .type =  ircmsg_get_type,
    .command = ircmsg_get_command,
    .nick = ircmsg_get_nick,
    .real = ircmsg_get_real,
    .host = ircmsg_get_host,
    .text = ircmsg_get_text,
    .cid = ircmsg_get_cid,
    .numeric = ircmsg_get_numeric,

    /* Get parameter at index 'argc' */
    .argv = ircmsg_get_argv,

    /* Dump output */
    .fdump = ircmsg_print
};


/******************************************************************************
 * TEST HARNESS 
 ******************************************************************************/
#if 0
int main(int argc, char ** argv)
{
    struct ircmsg * msg;

    for (--argc; argc > 0; --argc) {
        msg = ircmsg_parse(argv[argc]);
        ircmsg_print(msg);
        ircmsg_free(msg);
    }

    return 0;
}
#endif


/* end ircmsg.c */
