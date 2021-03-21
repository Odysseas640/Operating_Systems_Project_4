CC=g++
CFLAGS=-Wall -ggdb3

all: quic list.o

list.o: list.cpp
	$(CC) $(CFLAGS) -c list.cpp

quic.o: quic.cpp
	$(CC) $(CFLAGS) -c quic.cpp

quic: quic.o list.o
	$(CC) $(CFLAGS) -o quic quic.o list.o

.PHONY: clean

clean:
	rm -f quic.o list.o quic