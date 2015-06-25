#ifndef MOD_PERL_HEADER__H_
#define MOD_PERL_HEADER__H_

/* Perl interpreter state struct */
struct mp {
    const char * boot; /* Path to boot script */
    short int init; /* Is my_perl initialized? */
};

void * mod_perl_dispatch(void * obj);
void mod_perl_reinit(void);
void mod_perl_destroy(struct mp * state);
void mod_perl_init(struct mp * state);
const char * mod_perl_conf_get(const char * key, size_t len);
void mod_perl_conf_foreach(const char * key, size_t len, void (*cb)(struct keydata key, const char * val));

#endif
