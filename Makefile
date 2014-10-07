PKGS=sdl2 glew glu gl libpng16 lua
CC=clang
CFLAGS=-Ofast -Wall -std=c99 $(shell pkg-config $(PKGS) --cflags) -Ilibtess2
#CFLAGS=-g -O0 -Wall -std=c99 $(shell pkg-config $(PKGS) --cflags) -Ilibtess2
LINK=$(shell pkg-config $(PKGS) --libs) -lm

all: finished

palette_table_generator.o: palette_table_generator.c mud.h
	$(CC) $(CFLAGS) -c palette_table_generator.c

palette_table_generator: palette_table_generator.o mud.o a.o m.o
	$(CC) $(LINK) palette_table_generator.o mud.o a.o m.o -o palette_table_generator

dgfx/palette_table.png: palette_table_generator Makefile
	mkdir -p dgfx
	./palette_table_generator gfx/flat0.png | gm convert -size 256x16 -depth 8 rgb:- dgfx/palette_table.png

dgfx: dgfx/palette_table.png

res: dgfx

names.o: names.c names.h
	$(CC) $(CFLAGS) -c names.c

a.o: a.c a.h
	$(CC) $(CFLAGS) -c a.c

m.o: m.c m.h
	$(CC) $(CFLAGS) -c m.c

render.o: render.c render.h shader.h names.h
	$(CC) $(CFLAGS) -c render.c

mud.o: mud.c mud.h
	$(CC) $(CFLAGS) -c mud.c

font.o: font.c font.h mud.h shader.h a.h
	$(CC) $(CFLAGS) -c font.c

shader.o: shader.c shader.h
	$(CC) $(CFLAGS) -c shader.c

lvl.o: lvl.c lvl.h
	$(CC) $(CFLAGS) -c lvl.c

llvl.o: llvl.c llvl.h lvl.h
	$(CC) $(CFLAGS) -c llvl.c

finished.o: finished.c
	$(CC) $(CFLAGS) -c finished.c

finished: res names.o render.o mud.o font.o shader.o lvl.o llvl.o m.o a.o finished.o
	$(CC) $(LINK) names.o render.o mud.o font.o shader.o lvl.o llvl.o m.o a.o finished.o libtess2/libtess2.a -o finished

clean:
	rm -rf *.o finished dgfx/*

backup:
	tar cjf ../cdeeper.tar.bz2 .
	openssl enc -bf -in ../cdeeper.tar.bz2 -out ../cdeeper.tar.bz2.enc
	scp ../cdeeper.tar.bz2.enc ford.colourbox.com:
