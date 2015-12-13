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

#include <con.h>
#include <irc.h> 
#include <mod.h> /* for mod_initialize() which parses our config also */
#include <log.h>

int readcb(struct con * con, const char * s, void * userdata)
{
	struct server * server = userdata;
    /* Allocate new event and process it (in a separate thread .. ) */
    (void)mod_round_robin(server, s);

    return 1;
}

int eventcb(struct con * con, short event, void * userdata)
{
    struct server * server = userdata;

    switch(event) {
        case CON_EVENT_CONNECTED:
            Con.printf(con, "USER %s %s * :%s\r\n", server->nick, server->user, "machine-0.666");
            Con.printf(con, "NICK %s\r\n", server->nick);
            break;
        case CON_EVENT_EOF:
            fprintf(stderr, "Disconnected...?\n");
            break;
    }
    return 1;
}


void servercb(struct server * server) {
    enum con_flags flags = CF_RECONNECT;
    struct con * con;

	log_debug("[cxn] server-name: %s host: %s port: %d ssl: %d nick: %s user: %s\n",
			server->name, server->host, server->port, server->use_ssl, server->nick, server->user
	);

	if (!server->name) {
		log_debug("[cxn] Failed to add server with no name!");
		return;
	}

	if (server->use_ssl)
		flags |= CF_SSL;
	/* Set server as callback data */
	con = Con.new(server->host, server->port, flags, server);

	Con.callbacks(con, readcb, NULL, eventcb);
	Con.connect(con, gconfig.evbase);

	/* Add the connection to the server, this is crucial */
	server->con = con;

	htable.store(gconfig.servers, server->name, server);
}

/*
   TODO This should be moved into its own module along with the 
   struct server * handling code should be organized a little
   better
 */
void server_free(const char * key, void * value) {
	struct server * server = value;
	if (server) {
		Con.free(server->con);
		free(server);
	}
}

int main(int argc, char ** argv)
{
	/* Initialize our global event base and server list */
    gconfig.servers = htable.new(1000); // We could have up to a thousand servers 
    gconfig.evbase  = event_base_new();

	/* Load our perl workers */
    mod_initialize(gconfig.evbase);

	/* Read in the servers from the config */
	mod_conf_init();
	mod_conf_servers(servercb);

    /* Begin our loop */
    event_base_dispatch(gconfig.evbase);

    /* Cleanup */
	/* TODO rewrite gconfig.servers to be a list of struct server objects
	   so we can keep track of the config which is now not getting freed
	   and is a memory leak (but a static one so not super big deal)
	 */
    event_base_free(gconfig.evbase);

	htable.free_cb(gconfig.servers, server_free);

    mod_shutdown();

    puts("[--- Returning to OS control ---]");

    return 0;
}
