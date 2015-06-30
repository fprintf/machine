/*
 * Defines the module glue bindings
 * between perl (possibly other languages later) or C code modules
 * and the main core program
 */
#include <assert.h>

#include <EXTERN.h>
#include <perl.h>

#include "../config.h"    /* We load and parse the config in perl here */
#include "../irc.h"
#include "../xstr.h"
#include "../log.h"

#define MOD_PERL_BOOT_DFLT "mod_perl/boot.pl"
#define MOD_PERL_CONF_NAME "mod_perl::config::conf"
/* Beware side-effects */
#define mod_newSVpv(s) ( newSVpvn((s), (s) ? strlen(s) : 0) )

EXTERN_C void xs_init (pTHX);

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

EXTERN_C void
xs_init(pTHX)
{
	char *file = __FILE__;
	dXSUB_SYS;

	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}


static PerlInterpreter * my_perl; /* Perl(older) API requires this be called my_perl (can you believe this?) */
static struct mp {
    const char * boot; /* Path to boot script */
    short int init; /* Is my_perl initialized? */
} mp_state = {
    .boot = MOD_PERL_BOOT_DFLT,
    .init = 0,
};

static void mod_perl_destroy(struct mp * state)
{
    if (state->init) {
        /* Allow subsequent create/destroy of perl interpreter 
         * Must be set twice, construct sets it back to 0 so we need to set to 1 again */
        PL_perl_destruct_level = 1;
        /* Destroy */
        perl_destruct(my_perl);
        perl_free(my_perl);
        my_perl = NULL;
        state->init = 0;
    }
}

void mod_perl_init(struct mp * state)
{
	/* parse this init script */
	char * parg[8] = {"", (char *)state->boot};

    /* We're already init...re-init */
    if (state->init) 
        mod_perl_destroy(state);

    state->init = 1;
	my_perl = perl_alloc();
    /* Allow subsequent create/destroy of perl interpreter */
    PL_perl_destruct_level = 1;
    /* Create */
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
	perl_construct(my_perl);
	perl_parse(my_perl, xs_init, sizeof parg / sizeof parg[0], parg, NULL);
	perl_run(my_perl);
}

static void mod_perl_reinit_state(struct mp * state)
{
	mod_perl_destroy(state);
	mod_perl_init(state);
}


/*
 * Converts IRC event object to perl IRC object
 * (as defined by IRC perl module)
 */
static SV * mod_perl_to_ircobj(struct irc * event)
{
	SV * obj;

	if (event) {
		obj = newSV(0);
		sv_setref_pv(obj, "IRC", event);
		//fprintf(stderr, "added %p as 'IRC' object\n", (void *)event);
	}

	return obj;
}

static SV * mod_perl_call(SV * href, SV * sub)
{
	dSP;
	ENTER;
	SAVETMPS;

	/* Push our message hash ref onto the stack */
	PUSHMARK(SP);
	XPUSHs(sv_2mortal(newSVsv(href)));
	PUTBACK;

    /* Actually call our function with no return expected */
	call_sv(sub, G_EVAL|G_SCALAR|G_DISCARD);
	SPAGAIN;
	/* Check the eval first */
	if (SvTRUE(ERRSV))
	{
		fprintf(stderr, "mod_dispatch_perl(%s): %s", SvPV_nolen(sub), SvPV_nolen(ERRSV));
	}
	FREETMPS;
	LEAVE;

	return sub;
}

/* Simple func to call a perl sub passed in
   with the href */
static SV * mod_perl_call_simple(SV * href, const char * sub)
{
	SV * nsv;
	nsv = mod_newSVpv(sub);
	mod_perl_call(href, nsv);
	return nsv;
}
/*
 *  Got numeric event, lookup 'raw_NUM' as command sub
 */
static SV * mod_perl_call_numeric_simple(SV * href, short sub)
{
    SV * nsv = NULL;
    char * s = NULL;

    xsprintf(&s, "raw_%03d", sub);
    if (s) {
        nsv = mod_newSVpv(s);
        mod_perl_call(href, nsv);
        free(s);
    }
    return nsv;
}


/********************************************************************************************
 * INTERFACE
 ********************************************************************************************/
void mod_perl_reinit(void)
{
    mod_perl_reinit_state(&mp_state);
}

void mod_perl_shutdown(void)
{
    mod_perl_destroy(&mp_state);
}

/* Get config value (returns a const string) */
const char * mod_perl_conf_get(const char * key, size_t len)
{
    const char * p = NULL;
    HV * hv;

    hv = get_hv(MOD_PERL_CONF_NAME, 0);
    if (!hv) {
        fprintf(stderr,"Error, %%mod_perl::config::conf hash not found!\n");
    } else {
        SV ** entry = hv_fetch(hv, key, len, 0);

        /* Get the string in the SV** */
        if (entry && SvPOKp(*entry))
            p = SvPV_nolen(*entry);
    }

    return p;
}

static const char * hash_getstr(HV * hash, const char * key) {
	SV ** entry = hv_fetch(hash, key, strlen(key), 0);
	if (!entry || !SvPOK(*entry)) {
		log_debug("Missing %s fatal error.", key);
		return NULL;
	}

	return SvPV_nolen(*entry);
}

static int hash_getint(HV * hash, const char * key) {
	SV ** entry = hv_fetch(hash, key, strlen(key), 0);
	if (!entry || !SvIOK(*entry)) {
		log_debug("Missing %s fatal error.", key);
		return 0;
	}

	return SvIV(*entry);
}

/*
 * Translate the key/values from the server hash
 * into a struct, returns an initialized struct
 * fill with the values from the server hash or
 * an empty one with all members NULL/0 if there
 * are any failures.
 */
static struct server * mod_perl_conf_server(HV * hash) {
	struct server * server;

	if ( !(server = malloc(sizeof *server)) ) 
		return server;

	/* Ensure an empty object before we start assigning */
	*server = (struct server){};
	server->host = hash_getstr(hash, "host");
	server->port = hash_getint(hash, "port");
	server->use_ssl = hash_getint(hash, "ssl");

	server->nick = hash_getstr(hash, "nick");
	server->user = hash_getstr(hash, "user");

	return server;
}

/*
 * Read the servers list from the config and run our hooks 
 */
void mod_perl_conf_servers(void (*cb)(struct server *)) {
	const char * evalstr = "$" MOD_PERL_CONF_NAME "{servers}";
	//log_debug("running perl [%s]\n", evalstr);
	SV * ref = eval_pv(evalstr, 1);

	if (!ref || !SvROK(ref)) {
		fprintf(stderr, "Error: scalar returned from mod_perl_conf_servers() was not a reference\n");
		return;
	}

	/* Process each member of the server list and call our callback with a fully
	 * constructed 'struct server_conf' object we derive from the servers in the 
	 * server list
	 */
	HV * hash = (HV *)SvRV(ref);
	int32_t i, key_count = hv_iterinit(hash);
    for (i = 0; i < key_count; ++i) {
		char * key = NULL;
		int32_t keylen = 0;
		ref = hv_iternextsv(hash, &key, &keylen);

		/* If we got a hashref, build an object from its values and pass it to the callback */
		if (SvROK(ref)) {
			/* CALLER MUST FREE server object! */
			struct server * server = mod_perl_conf_server( (HV *)SvRV(ref) );
			cb(server);
		}
	}

}


/*
 * DEPRECATED
 * DO NOT USE will be removing this function entirely after i am done referencing it
 */
void mod_perl_conf_foreach(const char * key, size_t len, void (*cb)(const char * key, const char * val))
{
    HV * hv = get_hv(MOD_PERL_CONF_NAME, 0);
    HV * hash;
    AV * array;
    SV ** entry;


    if (!hv)
        return;
    entry = hv_fetch(hv, key, len, 0);

    if (entry && SvROK(*entry)) {
        switch(SvTYPE(SvRV(*entry))){
            case SVt_PVAV: /* Array ref */
                array = (AV *)SvRV(*entry);
                int32_t i, max;

                for (max = av_len(array), i = 0; i <= max; ++i) {
                    const char * val = NULL;

                    entry = av_fetch(array, i, 0);
                    if (entry && SvPOKp(*entry)) {
                        val = SvPV_nolen(*entry);

                        fprintf(stderr, "Calling callback for array index [%d] -> %s\n", i, val);
                        cb(NULL, val);
                    }
                }

                break;
            case SVt_PVHV: /* Hash ref */
                hash = (HV *)SvRV(*entry);
                HE * he;

                for (hv_iterinit(hash), he = hv_iternext(hash); he; he = hv_iternext(hash)) {
                    size_t len;
                    char * key = hv_iterkey(he, (I32 *)&len);
                    SV * sv = hv_iterval(hash, he);
                    const char * val = NULL;

                    if (sv && SvPOKp(sv))
                        val = SvPV_nolen(sv);

                    /* Call callback for this value */
                    fprintf(stderr, "Calling callback for %s -> %s\n", key, val);
                    cb(key, val);
                } 

                break;

            default:
                break;
        }
    }
}

void * mod_perl_dispatch(void * ctx)
{
    struct irc * event = ctx;
    SV * ircobj;

    /* Make sure our perl interpreter is initialized */
    if (!mp_state.init) 
        mod_perl_init(&mp_state);

    ircobj = mod_perl_to_ircobj(event);
    if (!ircobj) {
        fprintf(stderr, "Failed to convert ircobj to perl class, skipping..\n");
        return 0;
    }

    /*
     * Currently we look up commands (in boot.pl)
     * by 'msg->command'. We could enhance this
     * but we do all the handling in boot.pl anyways
     *
     * Usually PRIVMSG, KICK, etc..
     *
     */

    if (irc.israw(event))
	    mod_perl_call_numeric_simple(ircobj, irc.numeric(event));
    else
	    mod_perl_call_simple(ircobj, irc.command(event));

    return NULL;
}



/* TEST */
#if 0
int main(void)
{
    mod_perl_init(&mp_state);
    mod_perl_destroy(&mp_state);
    return 0;
}

#endif
