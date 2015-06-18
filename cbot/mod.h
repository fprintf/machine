#ifndef MOD_DISPATCH_HEADER__H_
#define MOD_DISPATCH_HEADER__H_

void mod_round_robin(struct con * con, const char * line);
int mod_dispatch(struct irc * event);
int mod_initialize(struct event_base * evbase);
void mod_shutdown(void);
const char * mod_conf_get(const char * key);
void mod_conf_foreach(const char * key, void (*cb)(const char * key, const char * val));

#endif
