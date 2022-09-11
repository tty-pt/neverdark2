#include <arpa/inet.h>
#include <cglm/cglm.h>
#include <fcntl.h>
/* #include <GL/gl.h> */
/* #include <GL/glx.h> */
/* #include <GL/glu.h> */
#include <GL/freeglut.h>
#include <netdb.h>
/* #include <ode/ode.h> */
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "shared.h"

struct {
	vec3 position;
	versor rotation;
} cam;

struct sockaddr_in servaddr;
int sockfd;
int w = 640, h = 480;
int lx = -1, ly = -1;
int ltick;
int keymap[1 + (unsigned char) -1] = {
	[ 0 ... (unsigned char) -1] = 0,
};
int updated_cam = 1;

vec3 frontv = { 0, 0, -1.0f };
vec3 upv = { 0, 1.0f, 0 };

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
	/* glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA); */
	glutInitDisplayMode(GLUT_RGB);
	glutInitWindowSize(w, h);
	glutCreateWindow("Neverdark2 client");
	/* glMatrixMode(GL_PROJECTION); */
	/* glLoadIdentity(); */
	/* glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0); */
	/* gluLookAt(0, 0, -9, 0, 0, 0, 0, 1, 0); */
}

void
viewport_init(int nw, int nh) 
{
	vec3 lookat, up;

	w = nw;
	h = nh;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0f, (float) w / (float) h, 0.1f, 100.0f);
	glm_quat_rotatev(cam.rotation, frontv, lookat);
	glm_quat_rotatev(cam.rotation, upv, up);
	glm_vec3_add(cam.position, lookat, lookat);
	gluLookAt(cam.position[0], cam.position[1], cam.position[2],
		  lookat[0], lookat[1], lookat[2],
		  up[0], up[1], up[2]);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	warn("cam p(%f,%f,%f) la(%f,%f,%f) u(%f,%f,%f)\n",
	     cam.position[0], cam.position[1], cam.position[2],
	     lookat[0], lookat[1], lookat[2],
	     up[0], up[1], up[2]);
}

void
display() {
	/* glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); */
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

	glutSolidTeapot(.5);

	glFlush();
}

void
resize(int w, int h)
{
	if (h == 0)
		h = 1;
	glViewport(0, 0, w, h);
	viewport_init(w, h);
}

int
key_ldt(float *ldt, char key, int tick, float dt)
{
	if (keymap[(unsigned char) key]) {
		*ldt = (keymap[(unsigned char) key] > ltick)
			? (float) (tick - keymap[(unsigned char) key]) / 1000.0f
			: dt;

		return 1;
	}

	return 0;
}

void
cam_rotate(float angle, float axisx, float axisy, float axisz)
{
	versor q;
	glm_quat(q, angle, axisx, axisy, axisz);
	glm_quat_mul(cam.rotation, q, cam.rotation);
	updated_cam = 1;
}

void
cam_translate(float dx, float dy, float dz)
{
	vec3 a = { dx, dy, dz };
	glm_quat_rotatev(cam.rotation, a, a);
	glm_vec3_add(cam.position, a, cam.position);
	updated_cam = 1;
}

void
update() {
	int tick = glutGet(GLUT_ELAPSED_TIME);
	float dt = (float) (tick - ltick) / 1000.0f, ldt;

	/* warn("dt %d - %d = %f\n", tick, ltick, dt); */

	if (key_ldt(&ldt, 'd', tick, dt))
		cam_translate(10.0f * ldt, 0, 0);

	if (key_ldt(&ldt, 'D', tick, dt))
		cam_rotate(-1.6 * ldt, 0.0f, 1.0f, 0.0f);

	if (key_ldt(&ldt, 'a', tick, dt))
		cam_translate(-10.0f * ldt, 0, 0);

	if (key_ldt(&ldt, 'A', tick, dt))
		cam_rotate(1.6 * ldt, 0.0f, 1.0f, 0.0f);

	if (key_ldt(&ldt, 'w', tick, dt))
		cam_translate(0, 0, -10.0f * ldt);

	if (key_ldt(&ldt, 'W', tick, dt))
		cam_rotate(1.6 * ldt, 1.0f, 0.0f, 0.0f);

	if (key_ldt(&ldt, 's', tick, dt))
		cam_translate(0, 0, 10.0f * ldt);

	if (key_ldt(&ldt, 'S', tick, dt))
		cam_rotate(-1.6 * ldt, 1.0f, 0.0f, 0.0f);

	if (key_ldt(&ldt, 'q', tick, dt))
		cam_translate(0, 10.0f * ldt, 0);

	if (key_ldt(&ldt, 'Q', tick, dt))
		cam_rotate(1.6 * ldt, 0.0f, 0.0f, 1.0f);

	if (key_ldt(&ldt, 'e', tick, dt))
		cam_translate(0, -10.0f * ldt, 0);

	if (key_ldt(&ldt, 'E', tick, dt))
		cam_rotate(-1.6 * ldt, 0.0f, 0.0f, 1.0f);

	if (updated_cam) {
		viewport_init(w, h);
		glutPostRedisplay();
		updated_cam = 0;
	}

	glutTimerFunc(33, update, 0);
	ltick = tick;
}

void
cam_init() {
	cam.position[0] = 0;
	cam.position[1] = 0;
	cam.position[2] = 0;
	glm_quat(cam.rotation, glm_deg(0.0f), 0.0f, 1.0f, 0.0f);
}

void
mouse_pmove(int x, int y)
{
	if (lx != -1) {
		float dx = (float) x - lx;
		float dy = (float) y - ly;
		cam_rotate(glm_deg(-0.0005f * dx * 180.0f / (float) w), 0.0f, 1.0f, 0.0f);
		cam_rotate(glm_deg(-0.0005f * dy * 180.0f / (float) h), 1.0f, 0.0f, 0.0f);
	}
	lx = x;
	ly = y;
}

void
key_up(unsigned char key, int x, int y)
{
	keymap[key] = 0;
}

void
key_down(unsigned char key, int x, int y)
{
	keymap[key] = glutGet(GLUT_ELAPSED_TIME);
	if (!keymap[key])
		keymap[key] = 1;
}

int
main(int argc, char *argv[])
{
	sleep(1);
	sock_init();
	gl_init(argc, argv);
	cam_init();
	text_send("auth One=qovmjbl");
	ltick = glutGet(GLUT_ELAPSED_TIME);

	/* warn("sizeof(dReal) = %lu\n", sizeof(dReal)); */
	glutDisplayFunc(display);
	glutReshapeFunc(resize);
	glutTimerFunc(60, update, 0);
	glutPassiveMotionFunc(mouse_pmove);
	glutKeyboardUpFunc(key_up);
	glutKeyboardFunc(key_down);
	/* glEnable(GL_DEPTH_TEST); */
	viewport_init(w, h);
	/* glClearColor(0.0, 0.0, 0.0, 0.0); */
	glutMainLoop();

	close(sockfd);

	return 0;
}
