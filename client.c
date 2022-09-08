#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "shared.h"

int
main()
{
	int sockfd;
	struct sockaddr_in servaddr;
	/* struct hostent *server; */
	int opt;

	sleep(1);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd == -1) {
		perror("socket creation failed");
		return -1;
	}

	/* opt = 1; */
	/* if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt, sizeof(opt)) < 0) { */
	/* 	perror("setsockopt"); */
	/* 	return -1; */
	/* } */

	/* server = gethostbyname("127.0.0.1"); */
	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	/* bcopy((char *)server->h_addr, */ 
	/*       (char *)&servaddr.sin_addr.s_addr, */
	/*       server->h_length); */
	servaddr.sin_port = htons(1988);

	if (connect(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) != 0) {
		perror("couldn't connect to the server");
		return -1;
	}

	/* if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) { */
	/* 	perror("make_nonblocking: fcntl"); */
	/* 	return -1; */
	/* } */

	/* if (fcntl(sockfd, F_SETOWN, getpid()) == -1) { */
	/* 	perror("F_SETOWN socket"); */
	/* 	return -1; */
	/* } */

	char buf[64];
	snprintf(buf, sizeof(buf), "Hello world");

	send(sockfd, &buf, strlen(buf), 0);
	close(sockfd);
	warn("Wrote '%s' to server.\n", buf);

	return 0;
}
