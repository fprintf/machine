SRC=con.c xstr.c ircmsg.c irc.c mod.c config.c
DATAROOT=$(abspath $(shell ls -d ../))
SHAREDIR=$(DATAROOT)/share
SRCDIR=$(DATAROOT)/src
MODDIR=$(SRCDIR)/mod_perl
MODSRC=$(MODDIR)/mod_perl.c
MOD_BOOTSCRIPT_DIR=$(MODDIR)/
OBJ=$(patsubst %.c,%.o,$(SRC)) 
MODOBJ=$(patsubst %.c,%.o,$(MODSRC))
SLIB=libirc.so
CFLAGS=-ggdb -Wall -DNDEBUG -Wl,-rpath=$(SHAREDIR)/lib
PERLLIB=`perl -MExtUtils::Embed -e ccopts -e ldopts`
prog=machine
LIBEVENT_TARBALL=libevent-2.0.21-stable.tar.gz
LIBEVENT_DIR=libevent-2.0.21-stable
LIBEVENT=$(SHAREDIR)/lib/libevent.so
PERLMOD_DIR=$(SRCDIR)/IRC
PERLMOD=$(PERLMOD_DIR)/blib/lib/IRC.pm

# Targets which should always be made regardless if they dont need to be
.PHONY: clean

all: $(prog) $(SLIB) $(PERLMOD)

clean: 
	if [ -e "$(PERLMOD_DIR)/Makefile" ]; then cd $(PERLMOD_DIR); make clean; fi
	$(RM) -rf $(OBJ) $(MODOBJ) $(SLIB) $(prog) $(LIBEVENT_DIR)

$(prog): $(LIBEVENT) $(MODOBJ) $(OBJ) main.c
	$(CC) -o $@ $+ -L$(SHAREDIR)/lib -lvector -lhtable -lssl -lcrypto -levent -levent_openssl $(PERLLIB) $(CFLAGS) -I$(SRCDIR) -I$(SHAREDIR)/include

$(PERLMOD): $(PERLMOD_DIR)/Makefile;
	$(MAKE) -C $(PERLMOD_DIR)

$(PERLMOD_DIR)/Makefile: $(PERLMOD_DIR)/Makefile.PL;
	cd $(PERLMOD_DIR); perl -f Makefile.PL

$(LIBEVENT): $(LIBEVENT_TARBALL)
	tar -xf $(LIBEVENT_TARBALL)
	cd $(LIBEVENT_DIR); ./configure --prefix=$(SHAREDIR) &&  make && make install

$(SLIB): $(OBJ)
	$(CC) -o $@ -shared $+
	
$(MODOBJ): %.o: %.c
	$(CC) -o $@ -c $+ $(PERLLIB) $(CFLAGS) -DMOD_BOOTSCRIPT_DIR=$(MOD_BOOTSCRIPT_DIR) -I$(SHAREDIR)/include -fPIC -fomit-frame-pointer

$(OBJ): %.o: %.c
	$(CC) -o $@ -c $+ $(CFLAGS) -I$(SHAREDIR)/include -fPIC -fomit-frame-pointer


