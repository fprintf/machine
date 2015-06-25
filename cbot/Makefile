SRC=con.c xstr.c ircmsg.c irc.c mod.c config.c
MODSRC=mod_perl/mod_perl.c
OBJ=$(patsubst %.c,%.o,$(SRC)) 
MODOBJ=$(patsubst %.c,%.o,$(MODSRC))
SLIB=libirc.so
CFLAGS=-ggdb -Wall -DNDEBUG
PERLLIB=`perl -MExtUtils::Embed -e ccopts -e ldopts`
prog=client
LIBEVENT_TARBALL=libevent-2.0.21-stable.tar.gz
LIBEVENT_DIR=libevent-2.0.21-stable
LIBEVENT=../share/lib/libevent.so
DATAROOT=$(abspath $(shell ls -d ../))
PERLMOD_DIR=IRC
PERLMOD=$(PERLMOD_DIR)/blib/lib/IRC.pm

.PHONY: all clean distclean

all: $(prog) $(SLIB) $(PERLMOD)

# Doesn't clean libevent by default, use make distclean for that
clean: 
	$(RM) -rf $(OBJ) $(MODOBJ) $(SLIB) $(prog) 

distclean: clean 
	-cd $(LIBEVENT_DIR) && make clean
	-cd $(PERLMID_DIR) && make clean
	rm -rf $(LIBEVENT_DIR)

$(prog): $(LIBEVENT) $(MODOBJ) $(OBJ) main.c
	$(CC) -o $@ $+ -L../share/lib -lvector -lhtable -lssl -lcrypto -levent -levent_openssl $(PERLLIB) $(CFLAGS) -I. -I../share/include

$(PERLMOD): $(SLIB) 
	cd $(PERLMOD_DIR) && perl -f Makefile.PL && make

$(LIBEVENT): $(LIBEVENT_DIR)
	cd $(LIBEVENT_DIR) && ./configure --prefix=$(DATAROOT)/share &&  make && make install

$(LIBEVENT_DIR): $(LIBEVENT_TARBALL)
	tar -xf $(LIBEVENT_TARBALL)

$(SLIB): $(OBJ)
	$(CC) -o $@ -shared $+
	
$(MODOBJ): %.o: %.c
	$(CC) -o $@ -c $+ $(PERLLIB) $(CFLAGS) -I../share/include -fPIC -fomit-frame-pointer

$(OBJ): %.o: %.c
	$(CC) -o $@ -c $+ $(CFLAGS) -I../share/include -fPIC -fomit-frame-pointer


