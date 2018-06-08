/*
 * This file was generated automatically by ExtUtils::ParseXS version 3.34 from the
 * contents of IRC.xs. Do not edit this file, edit IRC.xs instead.
 *
 *    ANY CHANGES MADE HERE WILL BE LOST!
 *
 */

#line 1 "IRC.xs"
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <irc.h> /* irc.* interface */

typedef struct irc * IRC;

#include "const-c.inc"

#line 23 "IRC.c"
#ifndef PERL_UNUSED_VAR
#  define PERL_UNUSED_VAR(var) if (0) var = var
#endif

#ifndef dVAR
#  define dVAR		dNOOP
#endif


/* This stuff is not part of the API! You have been warned. */
#ifndef PERL_VERSION_DECIMAL
#  define PERL_VERSION_DECIMAL(r,v,s) (r*1000000 + v*1000 + s)
#endif
#ifndef PERL_DECIMAL_VERSION
#  define PERL_DECIMAL_VERSION \
	  PERL_VERSION_DECIMAL(PERL_REVISION,PERL_VERSION,PERL_SUBVERSION)
#endif
#ifndef PERL_VERSION_GE
#  define PERL_VERSION_GE(r,v,s) \
	  (PERL_DECIMAL_VERSION >= PERL_VERSION_DECIMAL(r,v,s))
#endif
#ifndef PERL_VERSION_LE
#  define PERL_VERSION_LE(r,v,s) \
	  (PERL_DECIMAL_VERSION <= PERL_VERSION_DECIMAL(r,v,s))
#endif

/* XS_INTERNAL is the explicit static-linkage variant of the default
 * XS macro.
 *
 * XS_EXTERNAL is the same as XS_INTERNAL except it does not include
 * "STATIC", ie. it exports XSUB symbols. You probably don't want that
 * for anything but the BOOT XSUB.
 *
 * See XSUB.h in core!
 */


/* TODO: This might be compatible further back than 5.10.0. */
#if PERL_VERSION_GE(5, 10, 0) && PERL_VERSION_LE(5, 15, 1)
#  undef XS_EXTERNAL
#  undef XS_INTERNAL
#  if defined(__CYGWIN__) && defined(USE_DYNAMIC_LOADING)
#    define XS_EXTERNAL(name) __declspec(dllexport) XSPROTO(name)
#    define XS_INTERNAL(name) STATIC XSPROTO(name)
#  endif
#  if defined(__SYMBIAN32__)
#    define XS_EXTERNAL(name) EXPORT_C XSPROTO(name)
#    define XS_INTERNAL(name) EXPORT_C STATIC XSPROTO(name)
#  endif
#  ifndef XS_EXTERNAL
#    if defined(HASATTRIBUTE_UNUSED) && !defined(__cplusplus)
#      define XS_EXTERNAL(name) void name(pTHX_ CV* cv __attribute__unused__)
#      define XS_INTERNAL(name) STATIC void name(pTHX_ CV* cv __attribute__unused__)
#    else
#      ifdef __cplusplus
#        define XS_EXTERNAL(name) extern "C" XSPROTO(name)
#        define XS_INTERNAL(name) static XSPROTO(name)
#      else
#        define XS_EXTERNAL(name) XSPROTO(name)
#        define XS_INTERNAL(name) STATIC XSPROTO(name)
#      endif
#    endif
#  endif
#endif

/* perl >= 5.10.0 && perl <= 5.15.1 */


/* The XS_EXTERNAL macro is used for functions that must not be static
 * like the boot XSUB of a module. If perl didn't have an XS_EXTERNAL
 * macro defined, the best we can do is assume XS is the same.
 * Dito for XS_INTERNAL.
 */
#ifndef XS_EXTERNAL
#  define XS_EXTERNAL(name) XS(name)
#endif
#ifndef XS_INTERNAL
#  define XS_INTERNAL(name) XS(name)
#endif

/* Now, finally, after all this mess, we want an ExtUtils::ParseXS
 * internal macro that we're free to redefine for varying linkage due
 * to the EXPORT_XSUB_SYMBOLS XS keyword. This is internal, use
 * XS_EXTERNAL(name) or XS_INTERNAL(name) in your code if you need to!
 */

#undef XS_EUPXS
#if defined(PERL_EUPXS_ALWAYS_EXPORT)
#  define XS_EUPXS(name) XS_EXTERNAL(name)
#else
   /* default to internal */
#  define XS_EUPXS(name) XS_INTERNAL(name)
#endif

#ifndef PERL_ARGS_ASSERT_CROAK_XS_USAGE
#define PERL_ARGS_ASSERT_CROAK_XS_USAGE assert(cv); assert(params)

/* prototype to pass -Wmissing-prototypes */
STATIC void
S_croak_xs_usage(const CV *const cv, const char *const params);

STATIC void
S_croak_xs_usage(const CV *const cv, const char *const params)
{
    const GV *const gv = CvGV(cv);

    PERL_ARGS_ASSERT_CROAK_XS_USAGE;

    if (gv) {
        const char *const gvname = GvNAME(gv);
        const HV *const stash = GvSTASH(gv);
        const char *const hvname = stash ? HvNAME(stash) : NULL;

        if (hvname)
	    Perl_croak_nocontext("Usage: %s::%s(%s)", hvname, gvname, params);
        else
	    Perl_croak_nocontext("Usage: %s(%s)", gvname, params);
    } else {
        /* Pants. I don't think that it should be possible to get here. */
	Perl_croak_nocontext("Usage: CODE(0x%" UVxf ")(%s)", PTR2UV(cv), params);
    }
}
#undef  PERL_ARGS_ASSERT_CROAK_XS_USAGE

#define croak_xs_usage        S_croak_xs_usage

#endif

/* NOTE: the prototype of newXSproto() is different in versions of perls,
 * so we define a portable version of newXSproto()
 */
#ifdef newXS_flags
#define newXSproto_portable(name, c_impl, file, proto) newXS_flags(name, c_impl, file, proto, 0)
#else
#define newXSproto_portable(name, c_impl, file, proto) (PL_Sv=(SV*)newXS(name, c_impl, file), sv_setpv(PL_Sv, proto), (CV*)PL_Sv)
#endif /* !defined(newXS_flags) */

#if PERL_VERSION_LE(5, 21, 5)
#  define newXS_deffile(a,b) Perl_newXS(aTHX_ a,b,file)
#else
#  define newXS_deffile(a,b) Perl_newXS_deffile(aTHX_ a,b)
#endif

#line 167 "IRC.c"

/* INCLUDE:  Including 'const-xs.inc' from 'IRC.xs' */


XS_EUPXS(XS_IRC_constant); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_constant)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "sv");
    PERL_UNUSED_VAR(ax); /* -Wall */
    SP -= items;
    {
#line 4 "./const-xs.inc"
#ifdef dXSTARG
	dXSTARG; /* Faster if we have it.  */
#else
	dTARGET;
#endif
	STRLEN		len;
        int		type;
	/* IV		iv;	Uncomment this if you need to return IVs */
	/* NV		nv;	Uncomment this if you need to return NVs */
	/* const char	*pv;	Uncomment this if you need to return PVs */
#line 192 "IRC.c"
	SV *	sv = ST(0)
;
	const char *	s = SvPV(sv, len);
#line 18 "./const-xs.inc"
	type = constant(aTHX_ s, len);
      /* Return 1 or 2 items. First is error message, or undef if no error.
           Second, if present, is found value */
        switch (type) {
        case PERL_constant_NOTFOUND:
          sv =
	    sv_2mortal(newSVpvf("%s is not a valid IRC macro", s));
          PUSHs(sv);
          break;
        case PERL_constant_NOTDEF:
          sv = sv_2mortal(newSVpvf(
	    "Your vendor has not defined IRC macro %s, used",
				   s));
          PUSHs(sv);
          break;
	/* Uncomment this if you need to return IVs
        case PERL_constant_ISIV:
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          PUSHi(iv);
          break; */
	/* Uncomment this if you need to return NOs
        case PERL_constant_ISNO:
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          PUSHs(&PL_sv_no);
          break; */
	/* Uncomment this if you need to return NVs
        case PERL_constant_ISNV:
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          PUSHn(nv);
          break; */
	/* Uncomment this if you need to return PVs
        case PERL_constant_ISPV:
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          PUSHp(pv, strlen(pv));
          break; */
	/* Uncomment this if you need to return PVNs
        case PERL_constant_ISPVN:
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          PUSHp(pv, iv);
          break; */
	/* Uncomment this if you need to return SVs
        case PERL_constant_ISSV:
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          PUSHs(sv);
          break; */
	/* Uncomment this if you need to return UNDEFs
        case PERL_constant_ISUNDEF:
          break; */
	/* Uncomment this if you need to return UVs
        case PERL_constant_ISUV:
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          PUSHu((UV)iv);
          break; */
	/* Uncomment this if you need to return YESs
        case PERL_constant_ISYES:
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          PUSHs(&PL_sv_yes);
          break; */
        default:
          sv = sv_2mortal(newSVpvf(
	    "Unexpected return type %d while processing IRC macro %s, used",
               type, s));
          PUSHs(sv);
        }
#line 269 "IRC.c"
	PUTBACK;
	return;
    }
}


/* INCLUDE: Returning to 'IRC.xs' from 'const-xs.inc' */


XS_EUPXS(XS_IRC_target); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_target)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "event");
    {
	IRC	event;
	char *	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::target",
			"event", "IRC")
;
#line 21 "IRC.xs"
        RETVAL = irc.target(event);
#line 301 "IRC.c"
	sv_setpv(TARG, RETVAL); XSprePUSH; PUSHTARG;
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_text); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_text)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "event");
    {
	IRC	event;
	char *	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::text",
			"event", "IRC")
;
#line 29 "IRC.xs"
        RETVAL = irc.text(event);
#line 330 "IRC.c"
	sv_setpv(TARG, RETVAL); XSprePUSH; PUSHTARG;
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_nick); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_nick)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "event");
    {
	IRC	event;
	char *	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::nick",
			"event", "IRC")
;
#line 37 "IRC.xs"
        RETVAL = irc.nick(event);
#line 359 "IRC.c"
	sv_setpv(TARG, RETVAL); XSprePUSH; PUSHTARG;
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_real); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_real)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "event");
    {
	IRC	event;
	char *	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::real",
			"event", "IRC")
;
#line 45 "IRC.xs"
        RETVAL = irc.real(event);
#line 388 "IRC.c"
	sv_setpv(TARG, RETVAL); XSprePUSH; PUSHTARG;
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_host); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_host)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "event");
    {
	IRC	event;
	char *	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::host",
			"event", "IRC")
;
#line 53 "IRC.xs"
        RETVAL = irc.host(event);
#line 417 "IRC.c"
	sv_setpv(TARG, RETVAL); XSprePUSH; PUSHTARG;
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_servername); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_servername)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "event");
    {
	IRC	event;
	char *	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::servername",
			"event", "IRC")
;
#line 61 "IRC.xs"
        RETVAL = irc.server(event);
#line 446 "IRC.c"
	sv_setpv(TARG, RETVAL); XSprePUSH; PUSHTARG;
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_raw); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_raw)
{
    dVAR; dXSARGS;
    if (items != 2)
       croak_xs_usage(cv,  "event, text");
    {
	IRC	event;
	const char *	text = (const char *)SvPV_nolen(ST(1))
;
	int	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::raw",
			"event", "IRC")
;
#line 71 "IRC.xs"
        RETVAL = irc.raw(event, text);
#line 477 "IRC.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_say); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_say)
{
    dVAR; dXSARGS;
    if (items != 2)
       croak_xs_usage(cv,  "event, text");
    {
	IRC	event;
	const char *	text = (const char *)SvPV_nolen(ST(1))
;
	int	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::say",
			"event", "IRC")
;
#line 80 "IRC.xs"
        RETVAL = irc.say(event, text);
#line 508 "IRC.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_privmsg); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_privmsg)
{
    dVAR; dXSARGS;
    if (items != 3)
       croak_xs_usage(cv,  "event, target, text");
    {
	IRC	event;
	const char *	target = (const char *)SvPV_nolen(ST(1))
;
	const char *	text = (const char *)SvPV_nolen(ST(2))
;
	int	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::privmsg",
			"event", "IRC")
;
#line 90 "IRC.xs"
        RETVAL = irc.privmsg(event, target, text);
#line 541 "IRC.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_privmsg_server); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_privmsg_server)
{
    dVAR; dXSARGS;
    if (items != 4)
       croak_xs_usage(cv,  "event, server, target, text");
    {
	IRC	event;
	const char *	server = (const char *)SvPV_nolen(ST(1))
;
	const char *	target = (const char *)SvPV_nolen(ST(2))
;
	const char *	text = (const char *)SvPV_nolen(ST(3))
;
	int	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::privmsg_server",
			"event", "IRC")
;
#line 101 "IRC.xs"
        RETVAL = irc.privmsg_server(event, server, target, text);
#line 576 "IRC.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_connect); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_connect)
{
    dVAR; dXSARGS;
    if (items != 6)
       croak_xs_usage(cv,  "event, nick, user, host, port, ssl");
    {
	IRC	event;
	const char *	nick = (const char *)SvPV_nolen(ST(1))
;
	const char *	user = (const char *)SvPV_nolen(ST(2))
;
	const char *	host = (const char *)SvPV_nolen(ST(3))
;
	unsigned int	port = (unsigned int)SvUV(ST(4))
;
	int	ssl = (int)SvIV(ST(5))
;
	int	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::connect",
			"event", "IRC")
;
#line 114 "IRC.xs"
        RETVAL = irc.connect(event, nick, user, host, port, ssl);
#line 615 "IRC.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_reload); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_reload)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "event");
    {
	IRC	event;
	int	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::reload",
			"event", "IRC")
;
#line 122 "IRC.xs"
        RETVAL = irc.reload(event);
#line 644 "IRC.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS_EUPXS(XS_IRC_broadcast); /* prototype to pass -Wmissing-prototypes */
XS_EUPXS(XS_IRC_broadcast)
{
    dVAR; dXSARGS;
    if (items != 1)
       croak_xs_usage(cv,  "event");
    {
	IRC	event;
	int	RETVAL;
	dXSTARG;

	if (SvROK(ST(0)) && sv_derived_from(ST(0), "IRC")) {
	    IV tmp = SvIV((SV*)SvRV(ST(0)));
	    event = INT2PTR(IRC,tmp);
	}
	else
	    Perl_croak_nocontext("%s: %s is not of type %s",
			"IRC::broadcast",
			"event", "IRC")
;
#line 130 "IRC.xs"
		RETVAL = irc.broadcast(event);
#line 673 "IRC.c"
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}

#ifdef __cplusplus
extern "C"
#endif
XS_EXTERNAL(boot_IRC); /* prototype to pass -Wmissing-prototypes */
XS_EXTERNAL(boot_IRC)
{
#if PERL_VERSION_LE(5, 21, 5)
    dVAR; dXSARGS;
#else
    dVAR; dXSBOOTARGSXSAPIVERCHK;
#endif
#if (PERL_REVISION == 5 && PERL_VERSION < 9)
    char* file = __FILE__;
#else
    const char* file = __FILE__;
#endif

    PERL_UNUSED_VAR(file);

    PERL_UNUSED_VAR(cv); /* -W */
    PERL_UNUSED_VAR(items); /* -W */
#if PERL_VERSION_LE(5, 21, 5)
    XS_VERSION_BOOTCHECK;
#  ifdef XS_APIVERSION_BOOTCHECK
    XS_APIVERSION_BOOTCHECK;
#  endif
#endif

        newXS_deffile("IRC::constant", XS_IRC_constant);
        newXS_deffile("IRC::target", XS_IRC_target);
        newXS_deffile("IRC::text", XS_IRC_text);
        newXS_deffile("IRC::nick", XS_IRC_nick);
        newXS_deffile("IRC::real", XS_IRC_real);
        newXS_deffile("IRC::host", XS_IRC_host);
        newXS_deffile("IRC::servername", XS_IRC_servername);
        newXS_deffile("IRC::raw", XS_IRC_raw);
        newXS_deffile("IRC::say", XS_IRC_say);
        newXS_deffile("IRC::privmsg", XS_IRC_privmsg);
        newXS_deffile("IRC::privmsg_server", XS_IRC_privmsg_server);
        newXS_deffile("IRC::connect", XS_IRC_connect);
        newXS_deffile("IRC::reload", XS_IRC_reload);
        newXS_deffile("IRC::broadcast", XS_IRC_broadcast);
#if PERL_VERSION_LE(5, 21, 5)
#  if PERL_VERSION_GE(5, 9, 0)
    if (PL_unitcheckav)
        call_list(PL_scopestack_ix, PL_unitcheckav);
#  endif
    XSRETURN_YES;
#else
    Perl_xs_boot_epilog(aTHX_ ax);
#endif
}

