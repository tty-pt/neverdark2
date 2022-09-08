.SUFFIXES: .o .c

CFLAGS+=-Wall -I/usr/local/include
LDFLAGS+=-ldb -L/usr/local/lib

all: server client

hashtable.o: hashtable.c
server.o: server.c
client.o: client.c

.c.o:
	${COMPILE.c} -o $@ $<

server: hashtable.o server.o
	${LINK.c} -o $@ hashtable.o server.o

client: client.o
	${LINK.c} -o $@ client.o

clean:
	rm server.o hashtable.o server

.PHONY: all clean
