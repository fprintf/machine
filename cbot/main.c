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
#include <log.h>

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

	log_debug("[cxn] host: %s port: %d ssl: %d nick: %s user: %s\n",
			server->host, server->port, server->use_ssl, server->nick, server->user
	);

	if (server->use_ssl)
		flags |= CF_SSL;
	con = Con.new(server->host, server->port, flags, server);
	Con.callbacks(con, readcb, NULL, eventcb);
	Con.connect(con, gconfig.evbase);

	/* TODO TODO TODO
	 * NOTE: 'server' here is allocated and we need to free it, but we can't
	 yet, we need to keep track of it and free it later. The solution is to
	 add the con pointer into free and add the server item to the list
	 instead of the con (below) but we need to rewrite some things to make
	 that possible first, no biggie really but I just have to get around to it
	 */

	vector.push(gconfig.servers, con);
}

int main(int argc, char ** argv)
{
	/* Initialize our global event base and server list */
    gconfig.servers = vector.new(0);
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
	size_t i, max;
	for (i = 0, max = vector.size(gconfig.servers); i < max; ++i) {
		struct con * con = vector.index(gconfig.servers, i);
		Con.free(con);
	}
    vector.delete(gconfig.servers);

    mod_shutdown();

    puts("[--- Returning to OS control ---]");

    return 0;
}
