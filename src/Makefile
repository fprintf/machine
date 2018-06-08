SRC=con.c xstr.c ircmsg.c irc.c mod.c config.c
MODSRC=mod_perl/mod_perl.c
OBJ=$(patsubst %.c,%.o,$(SRC)) 
MODOBJ=$(patsubst %.c,%.o,$(MODSRC))
SLIB=libirc.so
CFLAGS=-ggdb -Wall -DNDEBUG -Wl,-rpath=../share/lib
PERLLIB=`perl -MExtUtils::Embed -e ccopts -e ldopts`
prog=client
LIBEVENT_TARBALL=libevent-2.0.21-stable.tar.gz
LIBEVENT_DIR=libevent-2.0.21-stable
LIBEVENT=../share/lib/libevent.so
DATAROOT=$(abspath $(shell ls -d ../))
PERLMOD_DIR=IRC
PERLMOD=$(PERLMOD_DIR)/blib/lib/IRC.pm

# Targets which should always be made regardless if they dont need to be
.PHONY: clean

all: $(prog) $(SLIB) $(PERLMOD)

clean: 
	if [ -e "$(PERLMOD_DIR)/Makefile" ]; then cd $(PERLMOD_DIR); make clean; fi
	$(RM) -rf $(OBJ) $(MODOBJ) $(SLIB) $(prog) $(LIBEVENT_DIR)

$(prog): $(LIBEVENT) $(MODOBJ) $(OBJ) main.c
	$(CC) -o $@ $+ -L../share/lib -lvector -lhtable -lssl -lcrypto -levent -levent_openssl $(PERLLIB) $(CFLAGS) -I. -I../share/include

$(PERLMOD): $(PERLMOD_DIR)/Makefile;
	cd $(PERLMOD_DIR); make

$(PERLMOD_DIR)/Makefile: $(PERLMOD_DIR)/Makefile.PL;
	cd $(PERLMOD_DIR); perl -f Makefile.PL

$(LIBEVENT): $(LIBEVENT_TARBALL)
	tar -xf $(LIBEVENT_TARBALL)
	cd $(LIBEVENT_DIR); ./configure --prefix=$(DATAROOT)/share &&  make && make install

$(SLIB): $(OBJ)
	$(CC) -o $@ -shared $+
	
$(MODOBJ): %.o: %.c
	$(CC) -o $@ -c $+ $(PERLLIB) $(CFLAGS) -I../share/include -fPIC -fomit-frame-pointer

$(OBJ): %.o: %.c
	$(CC) -o $@ -c $+ $(CFLAGS) -I../share/include -fPIC -fomit-frame-pointer


