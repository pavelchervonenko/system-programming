CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic \
         -fsanitize=address -fsanitize=leak -pthread

gcc: main.c
	$(CC) $(CFLAGS) main.c -o main

test-naive: gcc
	./main --philos 5 --meals 6 --mode naive

test-waiter: gcc
	./main --philos 5 --meals 6 --mode waiter

clean:
	rm -f main
