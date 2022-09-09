.SUFFIXES: .o .c

CFLAGS+=-Wall -I/usr/local/include
LDFLAGS+=-ldb -L/usr/local/lib

all: server client

server.o: server.c
client.o: client.c

.c.o:
	${COMPILE.c} -o $@ $<

server: server.o
	${LINK.c} -o $@ server.o

client: client.o
	${LINK.c} -o $@ client.o

clean:
	rm server.o server

.PHONY: all clean
