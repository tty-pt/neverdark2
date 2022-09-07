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

enum descr_flags {
	DF_CONNECTED = 1,
};

typedef struct object {
	double position[3];
	unsigned world;
	char *name;
} OBJ;

typedef struct descr_st {
	int fd, flags;
	OBJ *player;
} descr_t;

struct pkg_head {
	int type;
};

fd_set readfds, activefds, writefds;
descr_t descr_map[FD_SETSIZE];
int shutdown_flag = 0;
int sockfd;

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

	opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *) &server, sizeof(server))) {
		perror("binding stream socket");
		close(sockfd);
		exit(4);
	}

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_SET(sockfd, &readfds);
	descr_map[0].fd = sockfd;

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

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		perror("make_nonblocking: fcntl");
		abort();
	}

	return d;
}

void
descr_close(descr_t *d)
{
	if (d->flags & DF_CONNECTED) {
		warn("%s disconnects on fd %d\n",
		     d->player->name, d->fd);

		d->flags = 0;
		d->player = NULL;
	} else
		warn("%d never connected\n", d->fd);

	shutdown(d->fd, 2);
	close(d->fd);
	FD_CLR(d->fd, &activefds);
	if (d)
		memset(d, 0, sizeof(descr_t));
}

int
descr_read(descr_t *d)
{
	struct pkg_head head;
	int ret;

	ret = read(d->fd, &head, sizeof(head));
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
	register char c;
	int sockfd = make_socket(1988);
	struct timeval timeout;
	descr_t *d;
	int i;

	memset(descr_map, 0, sizeof(descr_map));

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGTERM, sig_shutdown);

	while (shutdown_flag == 0) {
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		readfds = activefds;
		int select_n = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);

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
