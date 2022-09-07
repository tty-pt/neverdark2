CFLAGS+=-Wall

server: server.c
	${LINK.c} -o $@ $<
