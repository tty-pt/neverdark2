.SUFFIXES: .o .c

CFLAGS += -Wall -I/usr/local/include -I/usr/X11R6/include
LDFLAGS += -L/usr/local/lib 

SERVER_LDFLAGS := -ldb
CLIENT_LDFLAGS := -L/usr/X11R6/lib -lX11 -lGL -lGLU -lglut -lm -lode -lcglm

all: server client

server.o: server.c
client.o: client.c

.c.o:
	${COMPILE.c} -o $@ $<

server: server.o
	${LINK.c} ${SERVER_LDFLAGS} -o $@ server.o

client: client.o
	${LINK.c} ${CLIENT_LDFLAGS} -o $@ client.o

clean:
	rm server.o server

.PHONY: all clean
