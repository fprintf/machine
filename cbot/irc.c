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

/* So we can update our user information */
#include "common.h"


enum irc_modes {
    IRC_M_NONE,
    IRC_M_ISOP    = 1 << 0,
    IRC_M_ISVOICE = 1 << 1,
    IRC_M_HALFOP  = 1 << 2,
    IRC_M_BANNED  = 1 << 3,
};

static const struct irc {
    struct con * con; /* Con.userdata(con) should provide any user/nicknames we might care about */
    struct ircmsg * msg;
    const char * server; /* Name of the server this message is from, for identification */
    unsigned long cid;   /* Connection ID  message came from */
} irc_initializer; /* static, all NULL */

/*  Getters */
static char * irc_target(struct irc * irc) 
{ 
    struct context * ctx = Con.userdata(irc->con, NULL);
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
static unsigned long irc_cid(struct irc * irc) { return Msg.cid(irc->msg); }  /* Return the connection cid for outgoing messages */
static int irc_israw(struct irc * irc) { return (Msg.type(irc->msg) == IRC_NUMERIC); }/* returns numeric of this IRC message */
static void irc_fdump(struct irc * irc, FILE * fp) { return Msg.fdump(irc->msg, fp); } /* dump message contents to FP */
/* Set and get the server name for this connection */
static const char * irc_setserver(struct irc * irc, const char * server) { return (irc->server = server); }
static const char * irc_server(struct irc * irc) { return irc->server; }

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
            mod_dispatch(event);
            break;

        case IRC_MODE:
            mod_dispatch(event);
            /* My mode changed, update it */
            if (0 == strcmp(Msg.argv(event->msg, 0), gconfig.nick)) {
                Msg.fdump(event->msg, stdout);
                /* TODO */
                /* Determine mode and set new event->modes */
                /* TODO */
            }
            break;

        case IRC_PING:
            /* Respond to ping as quickly as possible */
            log_debug("Sending PONG response: S%lu PONG :%s", irc_cid(event), Msg.text(event->msg));
            printf("S%lu PONG :%s\n", irc_cid(event), Msg.text(event->msg));
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

static struct irc * irc_parse(const char * line, struct con * con)
{
    struct irc * event = NULL;

    if ( (event = malloc(sizeof *event)) ) {
        *event = irc_initializer;
        event->con = con;
        event->msg = Msg.parse(line);
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

static struct irc * irc_dispatch(const char * line, struct con * con)
{
    struct irc * event = NULL;
    
    /* Process message */
    event = irc_parse(line, con);
    if (event) {
        irc_callback(event);
    } else {
        /* Parse failure on line */
        fprintf(stderr, "[error] Failed to parse line '%s'\n", line);
    }

    return NULL;
}

/* Output commands */
static int raw(struct irc * irc, const char * msg) { return printf("S%lu %s\n", irc->cid, msg); }
static int privmsg(struct irc * irc, const char * target, const char * msg) 
{ 
    return printf("S%lu PRIVMSG %s :%s\n", irc->cid, target, msg); 
}
static int cprivmsg(struct irc * irc, const char * target, const char * channel, const char * msg) 
{ 
    return printf("S%lu CPRIVMSG %s %s :%s\n", irc->cid, target, channel,msg); 
}
static int notice(struct irc * irc, const char * target, const char * msg) 
{ 
    return printf("S%lu NOTICE %s :%s\n", irc->cid, target, msg); 
}
static int say(struct irc * irc, const char * msg) 
{ 
    return printf("S%lu PRIVMSG %s :%s\n", irc->cid, !strncmp(Msg.argv(irc->msg,0),"#",1) ? Msg.argv(irc->msg,0) : Msg.nick(irc->msg), msg); 
}


/* Channel commands */
static int join(struct irc * irc, const char * target)
{
    return printf("S%lu JOIN %s\n", irc->cid, target);
}

static int joinkeys(struct irc * irc, const char * target, const char * keys)
{
    return printf("S%lu JOIN %s %s\n", irc->cid, target, keys);
}

static int part(struct irc * irc, const char * target, const char * msg)
{
    return printf("S%lu PART %s %s\n", irc->cid, target, msg ? msg : "");
}

static int topic(struct irc * irc, const char * target, const char *msg)
{
    return printf("S%lu TOPIC %s :%s\n", irc->cid, target, msg);
}

static int mode(struct irc * irc, const char * target, const char * flags, const char * args)
{
    return printf("S%lu MODE %s %s :%s\n", irc->cid, target, flags, args);
}

static int kick(struct irc * irc, const char * target, const char * ktarget, const char * msg)
{
    return printf("S%lu KICK %s %s :%s\n", irc->cid, target, ktarget, msg);
}

/* Info lookup */
static int whois(struct irc * irc, const char * target)
{
    return printf("S%lu WHOIS %s\n", irc->cid, target);
}
static int who(struct irc * irc, const char * mask)
{
    return printf("S%lu WHO %s\n", irc->cid, mask);
}
static int userhost(struct irc * irc, const char * users)
{
    return printf("S%lu USERHOST %s\n", irc->cid, users);
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
    .cid = irc_cid,
    .server = irc_server,
    .setserver = irc_setserver,

    .fdump = irc_fdump,

    .connect = irc_connect,
    .reload = irc_reload,

    /* Commands */
    .raw = raw,
    .privmsg = privmsg,
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


