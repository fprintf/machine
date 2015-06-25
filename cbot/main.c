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
#include <common.h>
#include <mod.h> /* for mod_initialize() which parses our config also */

int readcb(struct con * con, const char * s, void * userdata)
{
    /* Allocate new event and process it (in a separate thread .. ) */
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

void process_server(struct keydata key, const char * val) {
    switch(key.type) {
        case KEYDATA_INDEX:
            fprintf(stderr, "process_server got [%ld] = %s\n", key.index, val);
            break;
        case KEYDATA_STRING:
            fprintf(stderr, "process_server got %s = %s\n", key.key, val);
            break;
        default:
            fprintf(stderr, "process_server got unknown value type: %s\n", val);
            break;
    }
}

int main(int argc, char ** argv)
{
//    struct event * evsig;

    struct con * server;
    enum con_flags flags = CF_RECONNECT;

    struct context ctx;


    if (argc < 3) {
        fprintf(stderr,"usage: %s <host> <port> [use_ssl]\n", argv[0]);
        return 1;
    }

    /* Use ssl also */
    if (argc == 4) {
        flags |= CF_SSL;
    }


    gconfig.servers = vector.new(0);
    gconfig.evbase  = event_base_new();
    mod_initialize(gconfig.evbase);

    /* Get global nickname settings */
    ctx.nick = mod_conf_get("nickname");
    ctx.user = mod_conf_get("username");
    ctx.usermsg = "goto test;";
    /* Process each server now */
    mod_conf_foreach("servers", process_server);

    /* Setup new connection and add our read callback */
    server = Con.new(argv[1], atoi(argv[2]), flags, &ctx);
    Con.callbacks(server, readcb, NULL, eventcb);
    Con.connect(server, gconfig.evbase);
    /* Add server to our global server vector */
    vector.sindex(gconfig.servers, 0, server);
    fprintf(stdout, "DBG index: 0 server: %p\n", vector.index(gconfig.servers, 0));

    /* Begin our loop */
    event_base_dispatch(gconfig.evbase);

    /* Cleanup */
    Con.free(server);
    event_base_free(gconfig.evbase);
    vector.delete(gconfig.servers);
    // FIXME THIS IS CAUSING SEGFAULTS
    // FIXME It's actually workers_free()
    // FIXME we need to fix shutdown..
    // TODO Im already working on refactoring the code into its own file though
    // in process hopefully will discover the bug
    mod_shutdown();
    puts("[--- Returning to OS control ---]");

    return 0;
}
