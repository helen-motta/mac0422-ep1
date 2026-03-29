CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread

all: ep1 imesh

ep1: ep1.o
	$(CC) $(CFLAGS) -o $@ $^

imesh: imesh.o
	$(CC) $(CFLAGS) -o $@ $^ -lreadline -lhistory

%.o: %.c ep1.h imesh.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o ep1 imesh

.PHONY: all clean