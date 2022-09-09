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

#define TEXT_LEN 128
#define BUFFER_LEN 8192
#define BIGBUFSIZ 32768

typedef struct entity {
	struct sockaddr *addr;
	unsigned flags;
} ENT;

union object_specific {
	ENT entity;
};

typedef struct object {
	double position[3];
	unsigned world;
	char *name;

	union object_specific sp;
} OBJ;

typedef struct descr_st {
	struct sockaddr addr;
	OBJ *player;
} descr_t;

typedef struct {
	descr_t *descr;
	int argc;
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

int shutdown_flag = 0;
int sockfd;
unsigned select_tick = 0;

DB *cmdsdb = NULL;
DB *ddb = NULL;

core_command_t cmds[] = {
	/* { */
	/* 	.name = "auth", */
	/* 	.cb = &do_auth, */
	/* 	.flags = CF_NOAUTH, */
	/* } */
};

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

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

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

	listen(sockfd, 5);
	return sockfd;
}

core_command_t *
command_match(command_t *cmd) {
	return svhash_get(cmdsdb, cmd->argv[0]);
}

static command_t
command_new(descr_t *d, char *input, size_t len)
{
	command_t cmd;
	register char *p = input;

	p[len] = '\0';
	cmd.descr = d;
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

	descr_t *d = cmd->descr;

	if (!d->player) {
		if (cmd_i && (cmd_i->flags & CF_NOAUTH))
			cmd_i->cb(cmd);
		return;
	}

	OBJ *player = cmd->descr->player;
	ENT *eplayer = &player->sp.entity;

	command_debug(cmd, "command_process");

	// set current descriptor (needed for death)
	eplayer->addr = &cmd->descr->addr;

	if (cmd_i)
		cmd_i->cb(cmd);
}

static inline void
commands_init() {
	int i;

	db_init(&cmdsdb, DB_HASH);

	for (i = 0; i < sizeof(cmds) / sizeof(core_command_t); i++)
		svhash_put(cmdsdb, cmds[i].name, &cmds[i]);

}

int
main(int argc, char **argv)
{
	struct sockaddr_in cliaddr;
	int sockfd = make_socket(1988);
	struct pkg pkg;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGTERM, sig_shutdown);

	commands_init();
	db_init(&ddb, DB_BTREE);

	warn("listening\n");
	while (shutdown_flag == 0) {
		/* const buf[BUFSIZ]; */
		memset(&cliaddr, 0, sizeof(cliaddr));

		unsigned len = sizeof(cliaddr);

		/* if (recvfrom(sockfd, buf, sizeof(buf), 0, */
		if (recvfrom(sockfd, &pkg, sizeof(pkg), 0,
			 (struct sockaddr *) &cliaddr, (socklen_t *) &len) >= 0)
		{
			// received a message
			descr_t d;

			if (db_get(ddb, &d, &cliaddr, sizeof(cliaddr))) {
				// client not found in our local database
				d.player = NULL;
				memcpy(&d.addr, &cliaddr, sizeof(cliaddr));

				db_put(ddb, &cliaddr, sizeof(cliaddr), &d, sizeof(d));
			}

			// process message
			warn("Received package type %u\n", pkg.type);
			switch (pkg.type) {
			case PT_TEXT: {
				command_t cmd = command_new(&d, pkg.sp.text, strlen(pkg.sp.text));
				warn("TEXT: %s\n", pkg.sp.text);

				if (!cmd.argc)
					return 0;

				command_process(&cmd);
			}
			}
		}
	}
	return 0;
}
