#include <stdlib.h>
#include <stdbool.h>

#include "con.h"
#include "config.h"

/* GLOBAL CONFIG VARIABLE
 * INITIALIZATION
 */
struct config gconfig;


/*
 * struct server object (defined in config.h)
 *
 * You will need to access the server->con object
 * directly to access the underlying connection
 * for the server. There is also a 'name' and a
 * user/nickname for the server stored. The port
 * hostname and SSL are stored in the server->con
 * object underneath.
 *
 * So stuct server is guaranteed to have at minimum:
 * struct server {
 *     const char * name; // server name (from config) 
 *     const char * nickname; // nickname for this server (from config)
 *     const char * username; // username/realname for this server (from config..)
 *     struct con * con;  // underlying con object
 *     // likely others...
 * };
 *     
 */
struct server * make_server(const char * name) {
    struct server * server = malloc(sizeof *server);

    if (server) {
        *server = (struct server){.name = name};

        /* Initialize con object */
        server->con = Con.new(NULL, 6667, CF_RECONNECT, server);
    }

    return server;
}

void destroy_server(struct server ** server) {
    if (*server) {
        Con.free((*server)->con);
        free(*server);
        *server = NULL;
    }
}
