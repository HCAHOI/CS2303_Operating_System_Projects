CC=gcc

CFLAGS=-c -Wall

all: fs

fs: fs.o
	$(CC) fs.o -o fs $(LIBS)

fs.o: fs.c
	$(CC) $(CFLAGS) -g fs.c

clean:
	rm -rf *.o fs
