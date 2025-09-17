CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=address -fsanitize=leak
LDLIBS = -lrt

all: server client

server: server.o group.o
	$(CC) $(CFLAGS) -o server server.o group.o $(LDLIBS)

client: client.o group.o
	$(CC) $(CFLAGS) -o client client.o group.o $(LDLIBS)

server.o: server.c protocol.h group.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c protocol.h
	$(CC) $(CFLAGS) -c client.c

group.o: group.c group.h
	$(CC) $(CFLAGS) -c group.c

clean:
	rm -f server client *.o .srvpid in.txt

test1: server client
	printf "/etc/hosts\n/etc/passwd\n/bin/sh\n/bin/ls\n" > in.txt; \
	./server & echo $$! > .srvpid; sleep 1; \
	./client in.txt; \
	kill `cat .srvpid`; rm -f .srvpid in.txt

test2: server client
	printf "/etc/hosts\n/etc/passwd\n/bin/sh\n/bin/ls\n" > in.txt; \
	./server & echo $$! > .srvpid; sleep 1; \
	./client in.txt; \
	kill `cat .srvpid`; rm -f .srvpid

.PHONY: all clean test1 test2
