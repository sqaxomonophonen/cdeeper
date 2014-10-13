PKGS=sdl2 glew glu gl libpng16 lua
CC=clang
CFLAGS=-Ofast -Wall -std=c99 $(shell pkg-config $(PKGS) --cflags) -Ilibtess2
#CFLAGS=-g -O0 -Wall -std=c99 $(shell pkg-config $(PKGS) --cflags) -Ilibtess2
LINK=$(shell pkg-config $(PKGS) --libs) -lm
DERIVED=dgfx/palette_table.png lua/d/entities.lua

all: finished game

palette_table_generator.o: palette_table_generator.c mud.h
	$(CC) $(CFLAGS) -c palette_table_generator.c

palette_table_generator: palette_table_generator.o mud.o a.o m.o
	$(CC) $(LINK) palette_table_generator.o mud.o a.o m.o -o palette_table_generator

dgfx/palette_table.png: palette_table_generator Makefile
	mkdir -p dgfx
	./palette_table_generator gfx/ref.png | gm convert -size 256x16 -depth 8 rgb:- dgfx/palette_table.png

entities2lua.o: entities2lua.c entities.inc.h
	$(CC) $(CFLAGS) -c entities2lua.c

entities2lua: entities2lua.o
	$(CC) $(LINK) entities2lua.o -o entities2lua

lua/d/entities.lua: entities2lua
	mkdir -p lua/d
	./entities2lua > lua/d/entities.lua


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

runtime.o: runtime.c runtime.c
	$(CC) $(CFLAGS) -c runtime.c

finished.o: finished.c
	$(CC) $(CFLAGS) -c finished.c

finished: $(DERIVED) runtime.o names.o render.o mud.o font.o shader.o lvl.o llvl.o m.o a.o finished.o
	$(CC) $(LINK) runtime.o names.o render.o mud.o font.o shader.o lvl.o llvl.o m.o a.o finished.o libtess2/libtess2.a -o finished

game.o: game.c
	$(CC) $(CFLAGS) -c game.c

game: $(DERIVED) runtime.o names.o render.o mud.o font.o shader.o lvl.o llvl.o m.o a.o game.o
	$(CC) $(LINK) runtime.o names.o render.o mud.o font.o shader.o lvl.o llvl.o m.o a.o game.o libtess2/libtess2.a -o game

clean:
	rm -rf *.o finished dgfx/* lua/d/*.lua

backup:
	tar cjf ../cdeeper.tar.bz2 .
	openssl enc -bf -in ../cdeeper.tar.bz2 -out ../cdeeper.tar.bz2.enc
	scp ../cdeeper.tar.bz2.enc ford.colourbox.com:
