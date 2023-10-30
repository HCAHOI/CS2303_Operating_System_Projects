CC=gcc

CFLAGS=-c -Wall

all: disk

disk: disk.o
	$(CC) disk.o -o disk $(LIBS)

disk.o: disk.c
	$(CC) $(CFLAGS) -g disk.c

clean:
	rm -rf *.o disk
