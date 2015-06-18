/* Copyright 2010-2014 fprintf
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/bufferevent_struct.h>

#include <irc.h> /* includes con.h also */
#include <mod.h> /* for mod_initialize() which parses our config also */
#include <common.h>

int readcb(struct con * con, const char * s, void * userdata)
{
    /* Dispatch the line */
    (void)mod_round_robin(con, s);

    return 1;
}

int eventcb(struct con * con, short event, void * userdata)
{
    struct context * ctx = userdata;

    switch(event) {
        case CON_EVENT_CONNECTED:
            Con.printf(con, "USER %s %s * :%s\r\n", ctx->nick, ctx->user, ctx->usermsg);
            Con.printf(con, "NICK %s\r\n", ctx->nick);
            break;
        case CON_EVENT_EOF:
            fprintf(stderr, "Disconnected...?\n");
            break;
    }
    return 1;
}

void sigcallback(evutil_socket_t fd, short signum, void * arg)
{
    struct event_base * evbase = arg;
    fprintf(stderr, "Got interrupt..closing\n");
    event_base_loopexit(evbase, NULL);
}

int main(int argc, char ** argv)
{
    struct event * evsig;
    struct context ctx;
    ctx.nick = "thisiscool";
    ctx.user = "iz-bot";
    ctx.usermsg = "goto asshole;";

    gconfig.servers = vector.new(0);
    gconfig.evbase  = event_base_new();
    /* Setup signal trap */
    evsig = evsignal_new(gconfig.evbase, SIGINT, sigcallback, gconfig.evbase);
    event_add(evsig, NULL);

    /* Init modules */
    mod_initialize(gconfig.evbase);

    /* Begin our loop */
    event_base_dispatch(gconfig.evbase);

    /* Cleanup */
    event_base_free(gconfig.evbase);
    vector.delete(gconfig.servers);
    // FIXME THIS IS CAUSING SEGFAULTS
    // FIXME It's actually workers_free()
    // FIXME we need to fix shutdown..
    // TODO Im already working on refactoring the code into its own file though
    // in process hopefully will discover the bug
    mod_shutdown();

    puts("[--- Shutdown ---]");

    return 0;
}
