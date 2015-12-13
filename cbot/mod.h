#ifndef MOD_DISPATCH_HEADER__H_
#define MOD_DISPATCH_HEADER__H_

#include "config.h"

void mod_round_robin(struct server * server, const char * line);
int mod_dispatch(struct irc * event);
int mod_initialize(struct event_base * evbase);
void mod_shutdown(void);

/* module config */
int mod_conf_init(void);
int mod_conf_shutdown(void);
const char * mod_conf_get(const char * key);
void mod_conf_servers(void (*cb)(struct server *));

#endif
