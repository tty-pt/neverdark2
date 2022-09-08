#include <arpa/inet.h>
#include <ctype.h>
#include <db4/db.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include "shared.h"
#include "hashtable.h"

#define BUFFER_LEN 8192
#define BIGBUFSIZ 32768

typedef struct entity {
	int fd;
	unsigned flags;
} ENT;

union specific {
	ENT entity;
};

typedef struct object {
	double position[3];
	unsigned world;
	char *name;

	union specific sp;
} OBJ;

typedef struct descr_st {
	int fd;
	OBJ *player;
} descr_t;

typedef struct {
	OBJ *player;
	int fd, argc;
	char *argv[8];
} command_t;

typedef void core_command_cb_t(command_t *);

enum command_flags {
	CF_NOAUTH = 1,
	/* CF_EOL = 2, */
        CF_NOARGS = 4,
};

typedef struct {
	char *name;
	core_command_cb_t *cb;
	int flags;
} core_command_t;

struct pkg_head {
	int type;
};

fd_set readfds, activefds, xfds;
descr_t descr_map[FD_SETSIZE];
int shutdown_flag = 0;
int sockfd;
unsigned select_tick = 0;

DB *cmdsdb = NULL;

void sig_shutdown(int i)
{
	warn("SHUTDOWN: via SIGNAL");
	shutdown_flag = 1;
}

int
make_socket(int port)
{
	int opt;
	struct sockaddr_in server;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		perror("creating stream socket");
		exit(3);
	}

	opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	/* opt = 1; */
	/* if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt, sizeof(opt)) < 0) { */
	/* 	perror("setsockopt"); */
	/* 	exit(1); */
	/* } */

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *) &server, sizeof(server))) {
		perror("binding stream socket");
		close(sockfd);
		exit(4);
	}

	FD_ZERO(&readfds);
	FD_ZERO(&xfds);
	FD_SET(sockfd, &activefds);
	/* descr_map[0].fd = sockfd; */

	listen(sockfd, 5);
	return sockfd;
}

descr_t *
descr_new()
{
	struct sockaddr_in addr;
	socklen_t addr_len = (socklen_t)sizeof(addr);
	int fd = accept(sockfd, (struct sockaddr *) &addr, &addr_len);
	descr_t *d;

	if (fd <= 0) {
		perror("descr_new");
		return NULL;
	}

	warn("accept %d\n", fd);

	FD_SET(fd, &activefds);

	d = &descr_map[fd];
	memset(d, 0, sizeof(descr_t));
	d->fd = fd;
	d->player = NULL;

	/* if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) { */
	/* 	perror("make_nonblocking: fcntl"); */
	/* 	abort(); */
	/* } */

	/* if (fcntl(fd, F_SETOWN, getpid()) == -1) { */
	/* 	perror("F_SETOWN socket"); */
	/* 	abort(); */
	/* } */

	return d;
}

void
descr_close(descr_t *d)
{
	if (d->player) {
		warn("fd %d disconnects (%s)\n",
		     d->fd, d->player->name);

		/* d->flags = 0; */
		d->player = NULL;
	} else
		warn("fd %d diconnects\n", d->fd);

	shutdown(d->fd, 2);
	close(d->fd);
	FD_CLR(d->fd, &activefds);
	if (d)
		memset(d, 0, sizeof(descr_t));
}

core_command_t *
command_match(command_t *cmd) {
	return hash_get(cmdsdb, cmd->argv[0]);
}

static command_t
command_new(descr_t *d, char *input, size_t len)
{
	command_t cmd;
	register char *p = input;

	p[len] = '\0';
	cmd.player = d->player;
	cmd.fd = d->fd;
	cmd.argc = 0;

	if (!*p || !isprint(*p))
		return cmd;

	cmd.argv[0] = p;
	cmd.argc++;

	for (; p < input + len && *p != ' '; p++)
		;

	if (*p == ' ') {
		*p = '\0';
		cmd.argv[cmd.argc] = p + 1;
		core_command_t *cmd_i = command_match(&cmd);
		cmd.argc ++;

		if (cmd_i && cmd_i->flags & CF_NOARGS)
			goto cleanup;

		for (; p < input + len; p++) {
			if (*p != '=')
				continue;

			*p = '\0';

			cmd.argv[cmd.argc] = p + 1;
			cmd.argc ++;
		}
	}

cleanup:
	for (int i = cmd.argc;
	     i < sizeof(cmd.argv) / sizeof(char *);
	     i++)

		cmd.argv[i] = "";

	return cmd;
}

void
command_debug(command_t *cmd, char *label)
{
	char **arg;

	warn("command_debug '%s' %d", label, cmd->argc);
	for (arg = cmd->argv;
	     arg < cmd->argv + cmd->argc;
	     arg++)
	{
		warn(" '%s'", *arg);
	}
	warn("\n");
}

void
command_process(command_t *cmd)
{
	if (cmd->argc < 1)
		return;

	core_command_t *cmd_i = command_match(cmd);
	int descr = cmd->fd;

	descr_t *d = &descr_map[descr];

	if (!d->player) {
		if (cmd_i && (cmd_i->flags & CF_NOAUTH))
			cmd_i->cb(cmd);
		return;
	}

	OBJ *player = cmd->player;
	ENT *eplayer = &player->sp.entity;

	command_debug(cmd, "command_process");

	// set current descriptor (needed for death)
	eplayer->fd = descr;

	if (cmd_i)
		cmd_i->cb(cmd);
}

int
descr_read(descr_t *d)
{
	char buf[BUFFER_LEN];
	int ret;

	ret = recv(d->fd, buf, sizeof(buf), 0);
	switch (ret) {
	case -1:
		if (errno == EAGAIN)
			return 0;

		perror("descr_read");
		/* BUG("descr_read %s", strerror(errno)); */
	case 0:
	case 1:
		return -1;
	}
	ret -= 2;
	buf[ret] = '\0';

	command_t cmd = command_new(d, buf, ret);

	if (!cmd.argc)
		return 0;

	command_process(&cmd);

	return ret;
}

int
descr_read_oob(descr_t *d)
{
	struct pkg_head head;
	int ret;

	ret = recv(d->fd, &head, sizeof(head), MSG_OOB);
	switch (ret) {
	case -1:
		if (errno == EAGAIN)
			return 0;

		perror("descr_read");
		/* BUG("descr_read %s", strerror(errno)); */
	case 0:
	case 1:
		return -1;
	}

	return ret;
}

int
main(int argc, char **argv)
{
	int sockfd = make_socket(1988);
	struct timeval timeout;
	descr_t *d;
	int i;

	memset(descr_map, 0, sizeof(descr_map));

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGTERM, sig_shutdown);

	hash_init(&cmdsdb);

	warn("listening\n");
	while (shutdown_flag == 0) {
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		readfds = activefds;
		xfds = activefds;
		int select_n = select(FD_SETSIZE, &readfds, NULL, &xfds, &timeout);
		/* if (select_tick > 2) */
		/* 	return 0; */
		/* select_tick++; */

		switch (select_n) {
		case -1:
			switch (errno) {
			case EAGAIN:
				return 0;
			case EINTR:
				continue;
			}

			perror("select");
			return -1;
		case 0:
			continue;
		}

		for (d = descr_map, i = 0;
		     d < descr_map + FD_SETSIZE;
		     d++, i++) {
			if (FD_ISSET(i, &xfds) && i != sockfd) {
				if (descr_read_oob(d) < 0)
					descr_close(d);
			}

			if (!FD_ISSET(i, &readfds))
				continue;

			if (i == sockfd) {
				descr_new();
			} else if (descr_read(d) < 0)
				descr_close(d);
		}
	}
	return 0;
}
