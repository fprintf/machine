dirs = cbot 

all: $(dirs)
	cd $^ && make

clean: $(dirs)
	cd $^ && make clean

distclean: $(dirs)
	cd $^ && make distclean
