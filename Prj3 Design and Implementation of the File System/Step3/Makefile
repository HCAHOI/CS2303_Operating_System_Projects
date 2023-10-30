CC=gcc

CFLAGS=-c -Wall

#generate the executable file: disk fs client
all: disk fs client

disk: disk.o
	$(CC) disk.o -o disk $(LIBS)

disk.o: disk.c
	$(CC) $(CFLAGS) -g disk.c

fs: fs.o
	$(CC) fs.o -o fs $(LIBS)

fs.o: fs.c
	$(CC) $(CFLAGS) -g fs.c

client: client.o
	$(CC) client.o -o client $(LIBS)

client.o: client.c
	$(CC) $(CFLAGS) -g client.c

clean:
	rm -rf *.o disk fs client disk_storage
