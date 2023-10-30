CC=gcc

all: multi single

single: single.c
	$(CC) single.c -o single

multi: multi.c
	$(CC) multi.c -lpthread -o multi

clean:
	rm -rf single multi
