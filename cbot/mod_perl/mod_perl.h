#ifndef MOD_PERL_HEADER__H_
#define MOD_PERL_HEADER__H_

void * mod_perl_dispatch(void * obj);
void mod_perl_reinit(void);
const char * mod_perl_conf_get(const char * key, size_t len);
void mod_perl_conf_foreach(const char * key, size_t len, void (*cb)(const char * key, const char * val));

#endif
