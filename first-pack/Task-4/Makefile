CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic \
         -fsanitize=address -fsanitize=leak 
LDFLAGS = -lrt

all: server client

server: server.c puzzle.c puzzle.h common.h
	$(CC) $(CFLAGS) server.c puzzle.c -o server $(LDFLAGS)

client: client.c puzzle.c puzzle.h common.h
	$(CC) $(CFLAGS) client.c puzzle.c -o client $(LDFLAGS)

clean:
	rm -f server client *.o

test-ok: server client
	@./server & sleep 1; \
	 ./client 42 ok.txt; \
	 kill $$!

test-bad: server client
	@./server & sleep 1; \
	 ./client 77 bad.txt; \
	 kill $$!