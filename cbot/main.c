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
#include <stdlib.h>
#include <string.h>

#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/bufferevent_struct.h>

#include <irc.h> /* includes con.h also */
#include <common.h>
#include <mod.h> /* for mod_initialize() which parses our config also */
#include <config.h>

int readcb(struct con * con, const char * s, void * userdata)
{
    /* Allocate new event and process it (in a separate thread .. ) */
    (void)mod_round_robin(con, s);

    return 1;
}

int eventcb(struct con * con, short event, void * userdata)
{
    struct server * server = userdata;

    switch(event) {
        case CON_EVENT_CONNECTED:
            Con.printf(con, "USER %s %s * :%s\r\n", server->nickname, server->username, "dead");
            Con.printf(con, "NICK %s\r\n", server->nickname);
            break;
        case CON_EVENT_EOF:
            fprintf(stderr, "Disconnected...?\n");
            break;
    }
    return 1;
}

void process_server(struct keydata key, const char * val) {
    struct server * server = vector.index(gconfig.servers, vector.size(gconfig.servers) - 1);

    switch(key.type) {
        case KEYDATA_INDEX:
            /* Get last added server */
            if (!server)
                break;

            /* TODO nothing really to be done here right now */
            fprintf(stderr, "process_server got %s[%zd] = %s\n", key.parentkey, key.index, val);
            break;
        case KEYDATA_STRING:
            /* Start a new server, finish the old server */
            if (
                strncmp(val, "HASHREF", sizeof "HASHREF" - 1) == 0 || 
                strncmp(key.key, "FINAL", sizeof "FINAL" - 1) == 0
               ) {
                /* We have an existing server in play, finish it out */
                if (server) {
                    fprintf(stderr, "server: %s connecting\n", server->name);
                    /* Start our server connection */
                    fprintf(stderr, "host: %s:%d nick: %s user: %s\n",
                            Con.host(server->con, NULL), Con.port(server->con, 0),
                            server->nickname, server->username
                            );
                    Con.callbacks(server->con, readcb, NULL, eventcb);
                    Con.connect(server->con, gconfig.evbase);
                }
                /* Create a new server only if we're not completely done */
                if (strncmp(key.key, "FINAL", sizeof "FINAL" - 1) != 0) {
                    fprintf(stderr, "new server: %s\n", key.key);
                    server = make_server(key.key);
                    vector.push(
                            gconfig.servers, 
                            server
                            );
                }
                break; /* Done for this iteration */
            } 
            
            /* We don't have a server yet. ignore */
            if (!server) 
                break;

            /* Add the host to the server */
            if (strncmp(key.key, "host", sizeof "host" - 1) == 0) {
                fprintf(stderr, "server: %s host: %s\n", server->name, val);
                Con.host(server->con, val);
            } 
            /* port */
            else if (strncmp(key.key, "port", sizeof "port" - 1) == 0) {
                /* FIXME Don't use atoi() dude! could crash with overflow */
                int port = atoi(val);
                if (port >= 65535 || port <= 0) {
                    fprintf(stderr, "Invalid port: %d in parse, defaulting to: 6667\n", port);
                    port = 6667;
                }

                fprintf(stderr, "server: %s port: %d\n", server->name, port);
                Con.port(server->con, port);
            } 
            /* Enable SSL */
            else if (strncmp(key.key, "ssl", sizeof "ssl" - 1) == 0) {
                short tf = atoi(val);
                fprintf(stderr, "server: %s use_ssl: %d\n", server->name, tf);
                Con.fssl(server->con, tf);
            }
            /* Nickname */
            else if (strncmp(key.key, "nick", sizeof "nick" - 1) == 0) {
                server->nickname = val;
                fprintf(stderr, "server: %s nickname: %s\n", server->name, server->nickname);
            } 
            /* Username */
            else if (strncmp(key.key, "user", sizeof "user" - 1) == 0) {
                server->username = val;
                fprintf(stderr, "server: %s username: %s\n", server->name, server->username);
            } else {
                fprintf(stderr, "Warning: invalid option: %s\n", key.key);
            }
            break;
        default:
            fprintf(stderr, "process_server got unknown value type: %s\n", val);
            break;
    }
}

int main(int argc, char ** argv)
{
    /* Initialize our workers and our event base */
    gconfig.evbase  = event_base_new();
    mod_initialize(gconfig.evbase); /* Workers must be running before we start dispatching data */
    
    /* Init perl interpreter so we can parse the config */
    mod_conf_init();
    gconfig.servers = vector.new(0);
    /* Process each server now */
    mod_conf_foreach("servers", process_server);
    /* We're done parsing the config for now */
//    mod_shutdown();


    size_t i;
    for (i = vector.size(gconfig.servers) - 1; i != -1; --i) {
        struct server * server = vector.index(gconfig.servers, i);
        fprintf(stderr, "servers: S%d %s:%d %s!%s\n", i, Con.host(server->con, NULL), Con.port(server->con, 0), server->nickname, server->username);
    }
    /* Begin our loop */
    event_base_dispatch(gconfig.evbase);

    /* Cleanup */
    event_base_free(gconfig.evbase);
    /* Free all the servers we have running */
    for (i = vector.size(gconfig.servers) - 1; i != -1; --i)
        destroy_server(vector.index(gconfig.servers, i));
    vector.delete(gconfig.servers);
    mod_shutdown();
    puts("[--- Returning to OS control ---]");

    return 0;
}
