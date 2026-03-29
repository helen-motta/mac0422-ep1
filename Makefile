CC = gcc

all: ep1 imesh

ep1: ep1.o
	$(CC) -o $@ $^ -pthread

imesh: imesh.o
	$(CC) -o $@ $^ -lreadline -lhistory

%.o: %.c ep1.h imesh.h
	$(CC) -c $< -o $@

clean:
	rm -f *.o ep1 imesh

.PHONY: all clean