CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic \
         -fsanitize=address -fsanitize=leak -pthread

all: bathroom

bathroom: main.c bathroom.c
	$(CC) $(CFLAGS) main.c bathroom.c -o bathroom

run: bathroom
	./bathroom -n 2 -w 3 -m 3 -k 2

clean:
	rm -f bathroom
