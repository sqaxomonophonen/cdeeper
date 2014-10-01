PKGS=sdl2 glew glu gl libpng16 lua
CC=clang
CFLAGS=-Ofast -Wall -std=c99 $(shell pkg-config $(PKGS) --cflags) -Ilibtess2
#CFLAGS=-g -O0 -Wall -std=c99 $(shell pkg-config $(PKGS) --cflags) -Ilibtess2
LINK=$(shell pkg-config $(PKGS) --libs) -lm

all: finished

a.o: a.c a.h
	$(CC) $(CFLAGS) -c a.c

m.o: m.c m.h
	$(CC) $(CFLAGS) -c m.c

lvl.o: lvl.c lvl.h
	$(CC) $(CFLAGS) -c lvl.c

llvl.o: llvl.c llvl.h lvl.h
	$(CC) $(CFLAGS) -c llvl.c

finished.o: finished.c
	$(CC) $(CFLAGS) -c finished.c

finished: lvl.o llvl.o m.o a.o finished.o
	$(CC) $(LINK) lvl.o llvl.o m.o a.o finished.o libtess2/libtess2.a -o finished

clean:
	rm -rf *.o finished

backup:
	tar cjf ../cdeeper.tar.bz2 .
	openssl enc -bf -in ../cdeeper.tar.bz2 -out ../cdeeper.tar.bz2.enc
	scp ../cdeeper.tar.bz2.enc ford.colourbox.com:
