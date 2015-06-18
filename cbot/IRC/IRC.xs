#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <irc.h> /* irc.* interface */

typedef struct irc * IRC;

#include "const-c.inc"

MODULE = IRC		PACKAGE = IRC

INCLUDE: const-xs.inc

char *
target(event)
    IRC event
    CODE:
        RETVAL = irc.target(event);
    OUTPUT:
        RETVAL

char * 
text(event)
    IRC event
    CODE:
        RETVAL = irc.text(event);
    OUTPUT:
        RETVAL

char *
nick(event)
    IRC event
    CODE:
        RETVAL = irc.nick(event);
    OUTPUT:
        RETVAL

char *
real(event)
    IRC event
    CODE:
        RETVAL = irc.real(event);
    OUTPUT:
        RETVAL

char *
host(event)
    IRC event
    CODE:
        RETVAL = irc.host(event);
    OUTPUT:
        RETVAL


int
raw(event,text)
    IRC event
    const char * text
    CODE:
        RETVAL = irc.raw(event, text);
    OUTPUT:
        RETVAL

int
say(event,text)
    IRC event
    const char * text
    CODE:
        RETVAL = irc.say(event, text);
    OUTPUT:
        RETVAL

int
privmsg(event,target,text)
    IRC event
    const char * target
    const char * text
    CODE:
        RETVAL = irc.privmsg(event, target, text);
    OUTPUT:
        RETVAL

int
connect(event,nick,user,host,port,ssl)
    IRC event
    const char * nick
    const char * user
    const char * host
    unsigned int port
    int ssl
    CODE:
        RETVAL = irc.connect(event, nick, user, host, port, ssl);
    OUTPUT:
        RETVAL

int
reload(event)
    IRC event
    CODE:
        RETVAL = irc.reload(event);
    OUTPUT:
        RETVAL
