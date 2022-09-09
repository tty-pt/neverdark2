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
	struct pkg pkg;

	sleep(1);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd == -1) {
		perror("socket creation failed");
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(1988);

	/* if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) { */
	/* 	perror("make_nonblocking: fcntl"); */
	/* 	return -1; */
	/* } */

	pkg.type = PT_TEXT;
	snprintf(pkg.sp.text, sizeof(pkg.sp.text), "Hello world");

	sendto(sockfd, &pkg, sizeof(pkg), 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
	close(sockfd);
	warn("Wrote '%s' to server.\n", pkg.sp.text);

	return 0;
}
