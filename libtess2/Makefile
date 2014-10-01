all: libtess2.a

CFLAGS=-Ofast -Wall

bucketalloc.o: bucketalloc.c
	$(CC) $(CFLAGS) -c bucketalloc.c

dict.o: dict.c
	$(CC) $(CFLAGS) -c dict.c

geom.o: geom.c
	$(CC) $(CFLAGS) -c geom.c

mesh.o: mesh.c
	$(CC) $(CFLAGS) -c mesh.c

priorityq.o: priorityq.c
	$(CC) $(CFLAGS) -c priorityq.c

sweep.o: sweep.c
	$(CC) $(CFLAGS) -c sweep.c

tess.o: tess.c
	$(CC) $(CFLAGS) -c tess.c

libtess2.a: bucketalloc.o dict.o geom.o mesh.o priorityq.o sweep.o tess.o
	ar cr libtess2.a bucketalloc.o dict.o geom.o mesh.o priorityq.o sweep.o tess.o

test: libtess2.a test.c
	$(CC) $(CFLAGS) test.c libtess2.a -o test
