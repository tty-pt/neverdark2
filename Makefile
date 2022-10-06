.SUFFIXES: .o .c

CFLAGS += -O0 -g -Wall -I/usr/local/include -I/usr/X11R6/include
LDFLAGS += -L/usr/local/lib 

SERVER_LDFLAGS := -ldb
CLIENT_LDFLAGS := -L/usr/X11R6/lib -lX11 -lGL -lGLU -lglut -lm -lode -lcglm -lxxhash -ldb -lGLEW

all: server client

server.o: server.c
client.o: client.c
model.o: model.c
chunks.o: chunks.c
gl_global.o: gl_global.c

.c.o:
	${COMPILE.c} -o $@ $<

server: server.o
	${LINK.c} ${SERVER_LDFLAGS} -o $@ server.o

client: gl_global.o chunks.o model.o client.o
	${LINK.c} ${CLIENT_LDFLAGS} -o $@ gl_global.o model.o client.o chunks.o

clean:
	rm server.o server client.o client model.o chunks.o gl_global.o

.PHONY: all clean
