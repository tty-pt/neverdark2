#include <arpa/inet.h>
#include <fcntl.h>
/* #include <GL/gl.h> */
/* #include <GL/glx.h> */
/* #include <GL/glu.h> */
#include <GL/freeglut.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "shared.h"

struct sockaddr_in servaddr;
int sockfd;

static inline void
sock_init() {
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd == -1) {
		perror("socket creation failed");
		exit(-1);
	}

	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(1988);

	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
		perror("make_nonblocking: fcntl");
		exit(-1);
	}
}

inline static void
text_send(char *str) {
	struct pkg pkg;
	pkg.type = PT_TEXT;
	snprintf(pkg.sp.text, sizeof(pkg.sp.text) - 1, "%s", str);
	sendto(sockfd, &pkg, sizeof(pkg), 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
	warn("Wrote '%s' to server.\n", str);
}

inline static void
gl_init(int argc, char *argv[]) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_SINGLE);
	glutInitWindowSize(640, 480);
	glutCreateWindow("Neverdark2 client");
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
}

void
display() {

	// Clear all pixels
	glClear(GL_COLOR_BUFFER_BIT);

	// draw white polygon (rectangle) with corners 
	// at (0.25, 0.25, 0.0) and (0.75, 0.75, 0.0)
	glColor3f(1.0,1.0,1.0);
	glBegin(GL_POLYGON);
	glVertex3f(0.25, 0.25, 0.0);
	glVertex3f(0.75, 0.25, 0.0);
	glVertex3f(0.75, 0.75, 0.0);
	glVertex3f(0.25, 0.75, 0.0);
	glEnd();

	// Don't wait start processing buffered OpenGL routines
	glFlush();
}

int
main(int argc, char *argv[])
{
	sleep(1);
	sock_init();
	gl_init(argc, argv);
	text_send("Hello world");

	glutDisplayFunc(display);
	glutMainLoop();

	close(sockfd);

	return 0;
}
