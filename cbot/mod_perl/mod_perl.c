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

#include "mod_perl.h"

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
struct mp mp_state = {
    .boot = MOD_PERL_BOOT_DFLT,
    .init = 0,
};

void mod_perl_destroy(struct mp * state)
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

/* Get config value (returns a const string) */
const char * mod_perl_conf_get(const char * key, size_t len)
{
    const char * p = NULL;
    HV * hv;

    hv = get_hv(MOD_PERL_CONF_NAME, 0);
    if (!hv) {
        fprintf(stderr,"Error, %%mod_perl::config::conf hash not found!\n");
    } else {
        SV ** entry = hv_fetch(hv, key, (I32)len, 0);

        /* Get the string in the SV** */
        if (entry && SvPOKp(*entry))
            p = SvPV_nolen(*entry);
    }

    return p;
}

/* 
 * Recursive handlers for specific types
 */
void mod_perl_conf_array(AV * array, void (*cb)(struct keydata key, const char * val));
void mod_perl_conf_hash(HV * hash, void (*cb)(struct keydata key, const char * val));

int mod_perl_conf_recurse(SV * sv, void (*cb)(struct keydata key, const char * val))
{
    if (SvROK(sv)) 
        return 0;

    switch(SvTYPE(SvRV(sv))) {
        /* Array reference */
        case SVt_PVAV: 
            mod_perl_conf_array((AV *)SvRV(sv), cb);
            break;

            /* Hash reference */
        case SVt_PVHV:
            mod_perl_conf_hash((HV *)SvRV(sv), cb);
            break;
        default:
            fprintf(stderr, "Unhandled reference type\n");
            break;
    }

    return 1;
}

void mod_perl_conf_hash(HV * hash, void (*cb)(struct keydata key, const char * val))
{
    HE * he;

    for (hv_iterinit(hash), he = hv_iternext(hash); he; he = hv_iternext(hash)) {
        SV * sv = hv_iterval(hash, he);

        if (!sv)
            continue;

        if (mod_perl_conf_recurse(sv, cb)) {
            continue;
        } else if (SvPOKp(sv)) {
            size_t len;
            char * stringkey = hv_iterkey(he, (I32*)&len);
            const char * val = SvPV_nolen(sv);

            /* Call callback for this value */
            fprintf(stderr, "Calling callback for %s -> %s\n", stringkey, val);
            cb((struct keydata){.type = KEYDATA_STRING, .key = stringkey}, val);
        }
    } 

    /* end of function */
}

void mod_perl_conf_array(AV * array, void (*cb)(struct keydata key,  const char * val)) 
{
    int32_t i, max;
    SV ** entry;

    for (max = av_len(array), i = 0; i <= max; ++i) {
        entry = av_fetch(array, i, 0);
        if (!entry || !*entry) 
            continue;

        if (mod_perl_conf_recurse(*entry, cb)) {
            continue;
        } else if (SvPOKp(*entry)) { /* Simple scalar */
            const char * val = SvPV_nolen(*entry);

            fprintf(stderr, "Calling callback for array index [%d] -> %s\n", i, val);
            cb((struct keydata){.type = KEYDATA_INDEX, .index = i}, val);
        }
    }

    /* end of function */
}

void mod_perl_conf_foreach(const char * key, size_t len, void (*cb)(struct keydata key, const char * val))
{
    HV * hv = get_hv(MOD_PERL_CONF_NAME, 0);
    SV ** entry;

    if (!hv)
        return;
    entry = hv_fetch(hv, key, (I32)len, 0);

    if (!entry || !*entry) {
        fprintf(stderr, "Invalid config value: %s\n", key);
        return;
    }

    /* Start processing the entries recursively 
     * if that doesn't work see if its a normal scalar value
     */
    if (!mod_perl_conf_recurse(*entry, cb) && SvPOKp(*entry)) {
        const char * val = SvPV_nolen(*entry);

        fprintf(stderr, "Calling callback for %s -> %s\n", key, val);
        cb((struct keydata){.type = KEYDATA_STRING, .key = key}, val);
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
