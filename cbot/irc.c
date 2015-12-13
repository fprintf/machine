/*
==============================
Copyright (c) 2000-2007 Niels Provos <provos@citi.umich.edu>
Copyright (c) 2007-2012 Niels Provos and Nick Mathewson

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
==============================
*/
#include <stdlib.h>
#include <string.h>


#include "con.h"    /* connection handling interface */
#include "ircmsg.h" /* msg parsing interface */

#include "irc.h"    /* our own opaque type hiding all the above */
#include "mod.h"    /* module interface */
#include "log.h"


enum irc_modes {
    IRC_M_NONE,
    IRC_M_ISOP    = 1 << 0,
    IRC_M_ISVOICE = 1 << 1,
    IRC_M_HALFOP  = 1 << 2,
    IRC_M_BANNED  = 1 << 3,
};

static const struct irc {
    struct server * server; /* Con.userdata(con) should provide any user/nicknames we might care about */
    struct ircmsg * msg;
} irc_initializer; /* static, all NULL */

/*  Getters */
static char * irc_target(struct irc * irc) 
{ 
    struct server * ctx = irc->server;
    if (Msg.argv(irc->msg,0))
        return !strcmp(Msg.argv(irc->msg,0),ctx->nick) ? Msg.nick(irc->msg) : Msg.argv(irc->msg,0);
    else
        return Msg.text(irc->msg);
} /* nick or channel this was directed too */
static char * irc_text(struct irc * irc) { return Msg.text(irc->msg); } /* nick or channel this was directed too */
static char * irc_nick(struct irc * irc) { return Msg.nick(irc->msg); }  /* returns nick who sent this */
static char * irc_real(struct irc * irc) { return Msg.real(irc->msg); }  /* returns realname part of nick who sent this */
static char * irc_host(struct irc * irc) { return Msg.host(irc->msg); }  /* returns host part of nick who sent this */
static char * irc_command(struct irc * irc) { return Msg.command(irc->msg); }/* returns command name of this IRC message */
static short irc_numeric(struct irc * irc) { return Msg.numeric(irc->msg); }/* returns numeric of this IRC message */
static int irc_israw(struct irc * irc) { return (Msg.type(irc->msg) == IRC_NUMERIC); }/* returns numeric of this IRC message */
static void irc_fdump(struct irc * irc, FILE * fp) { return Msg.fdump(irc->msg, fp); } /* dump message contents to FP */
/* Set and get the server name for this connection */
static char * irc_server(struct irc * irc) { return Msg.servername(irc->msg); }

/*
 * Dispatch event to our modules/handlers
 */
static int irc_callback(struct irc * event)
{
    switch (Msg.type(event->msg)) {
        case IRC_UNSET:
        default:
//            Msg.fdump(event->msg, stdout);
            break;

        // Initialization event, this is where we load the config and connect to any initial servers
        case IRC_INIT:
            mod_dispatch(event);
            break;

        case IRC_NUMERIC:
			if (irc_numeric(event) == 1) {
				/* TODO Join the channels for this server */
			}

            mod_dispatch(event);
            break;

        case IRC_MODE:
            mod_dispatch(event);
#if 0 
			/* FIXME Got a segfault happening here, probably in the strcmp() or the Msg.fdump() */
            /* My mode changed, update it */
            if (0 == strcmp(Msg.argv(event->msg, 0), gconfig.nick)) {
                Msg.fdump(event->msg, stdout);
                /* TODO */
                /* Determine mode and set new event->modes */
                /* TODO */
            }
#endif
            break;

        case IRC_PING:
            /* Respond to ping as quickly as possible */
            log_debug("Sending PONG response: S%s PONG :%s", irc_server(event), Msg.text(event->msg));
            printf("S%s PONG :%s\n", irc_server(event), Msg.text(event->msg));
            break;

        case IRC_PRIVMSG:
            mod_dispatch(event);
            break;
        case IRC_NOTICE:
            mod_dispatch(event);
            break;
        case IRC_JOIN:
        case IRC_PART:
        case IRC_QUIT:
            mod_dispatch(event);
            break;
        case IRC_NICK:
//            fprintf(stderr, "-> %s changed nick to %s\n", Msg.nick(event->msg), Msg.text(event->msg));
            mod_dispatch(event);
            break;
        case IRC_KICK:
            mod_dispatch(event);
            break;
    }

    return 1;
}

static void irc_free(struct irc * event); /* Forward declaration */

static struct irc * irc_parse(const char * line)
{
    struct irc * event = NULL;

    if ( (event = malloc(sizeof *event)) ) {
        *event = irc_initializer;
        event->msg = Msg.parse(line);
		event->server = Msg.server(event->msg);
        if (!event->msg) {
            irc_free(event);
            event = NULL;
        }
    }

    return event;
}

/*
 * API FUNCTIONS
 */
static void irc_free(struct irc * event)
{
    if (event) {
        Msg.free(event->msg);
        free(event);
    }
}

static struct irc * irc_dispatch(const char * line)
{
    struct irc * event = NULL;
    
    /* Process message */
    event = irc_parse(line);
    if (event) {
        irc_callback(event);
    } else {
        /* Parse failure on line */
        fprintf(stderr, "[error] Failed to parse line '%s'\n", line);
    }

    return NULL;
}

/* Output commands */
static int raw(struct irc * irc, const char * msg) { return printf("S%s %s\n", irc_server(irc), msg); }
static int privmsg_server(struct irc * irc, const char * server, const char * target, const char * msg) 
{ 
    return printf("S%s PRIVMSG %s :%s\n", server, target, msg); 
}
static int privmsg(struct irc * irc, const char * target, const char * msg) 
{ 
    return printf("S%s PRIVMSG %s :%s\n", irc_server(irc), target, msg); 
}
static int cprivmsg(struct irc * irc, const char * target, const char * channel, const char * msg) 
{ 
    return printf("S%s CPRIVMSG %s %s :%s\n", irc_server(irc), target, channel,msg); 
}
static int notice(struct irc * irc, const char * target, const char * msg) 
{ 
    return printf("S%s NOTICE %s :%s\n", irc_server(irc), target, msg); 
}
static int say(struct irc * irc, const char * msg) 
{ 
    return printf("S%s PRIVMSG %s :%s\n", irc_server(irc), !strncmp(Msg.argv(irc->msg,0),"#",1) ? Msg.argv(irc->msg,0) : Msg.nick(irc->msg), msg); 
}


/* Channel commands */
static int join(struct irc * irc, const char * target)
{
    return printf("S%s JOIN %s\n", irc_server(irc), target);
}

static int joinkeys(struct irc * irc, const char * target, const char * keys)
{
    return printf("S%s JOIN %s %s\n", irc_server(irc), target, keys);
}

static int part(struct irc * irc, const char * target, const char * msg)
{
    return printf("S%s PART %s %s\n", irc_server(irc), target, msg ? msg : "");
}

static int topic(struct irc * irc, const char * target, const char *msg)
{
    return printf("S%s TOPIC %s :%s\n", irc_server(irc), target, msg);
}

static int mode(struct irc * irc, const char * target, const char * flags, const char * args)
{
    return printf("S%s MODE %s %s :%s\n", irc_server(irc), target, flags, args);
}

static int kick(struct irc * irc, const char * target, const char * ktarget, const char * msg)
{
    return printf("S%s KICK %s %s :%s\n", irc_server(irc), target, ktarget, msg);
}

/* Info lookup */
static int whois(struct irc * irc, const char * target)
{
    return printf("S%s WHOIS %s\n", irc_server(irc), target);
}
static int who(struct irc * irc, const char * mask)
{
    return printf("S%s WHO %s\n", irc_server(irc), mask);
}
static int userhost(struct irc * irc, const char * users)
{
    return printf("S%s USERHOST %s\n", irc_server(irc), users);
}

/* Management commands */
/* Connect to a new server */
static int irc_connect(struct irc * irc, const char * nick, const char * username, const char * host, unsigned int port, int ssl)
{
    return printf(":CONNECT %s %s %s %d %d\n", nick, username, host, port, ssl);
}
/* Connect to a new server */
static int irc_reload(struct irc * irc)
{
    return printf(":RELOAD\n");
}



const struct irc_api irc = {
    .dispatch = irc_dispatch,
    .free = irc_free,

    /* Get */
    .target = irc_target,
    .text = irc_text,
    .nick = irc_nick,
    .real = irc_real,
    .host = irc_host,
    .command = irc_command,
    .numeric = irc_numeric,
    .israw = irc_israw,
    .server = irc_server,

    .fdump = irc_fdump,

    .connect = irc_connect,
    .reload = irc_reload,

    /* Commands */
    .raw = raw,
    .privmsg = privmsg,
    .privmsg_server = privmsg_server,
    .cprivmsg = cprivmsg,
    .notice = notice,
    .say = say,
    .join = join,
    .joinkeys = joinkeys,
    .part = part,
    .topic = topic,
    .mode = mode,
    .kick = kick,

    /* Info lookup */
    .whois = whois,
    .who = who,
    .userhost = userhost,
};


