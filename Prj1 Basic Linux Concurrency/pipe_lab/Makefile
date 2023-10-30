CC=gcc

CFLAGS=-c -Wall

all: Copy Gen

Copy: Copy.c
	$(CC) Copy.c -o Copy

Gen: data_generator.c
	$(CC) data_generator.c -o Gen

clean:
	rm -rf Copy Gen
