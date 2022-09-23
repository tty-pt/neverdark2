#include <arpa/inet.h>
#include <cglm/cglm.h>
#include <ctype.h>
#include <fcntl.h>
/* #include <GL/gl.h> */
/* #include <GL/glx.h> */
/* #include <GL/glu.h> */
/* #include <GLFW/glfw3.h> */
#include <GL/freeglut.h>
#include <netdb.h>
/* #include <ode/ode.h> */
#include <stdlib.h>
#include <string.h>
/* #include <sys/mman.h> */
#include <sys/socket.h>
/* #include <sys/stat.h> */
#include <sys/time.h>
#include <unistd.h>
#include "model.h"
#include "shared.h"
#include "chunks.h"
#include "debug.h"

struct {
	vec3 position;
	versor rotation;
} cam;

struct sockaddr_in servaddr;
int sockfd;
int w = 640, h = 480;
int lx = -1, ly = -1;
long long unsigned ltick;
long long unsigned keymap[1 + (unsigned char) -1] = {
	[ 0 ... (unsigned char) -1] = 0,
};
int updated_cam = 1;

vec3 frontv = { 0, 0, -1.0f };
vec3 upv = { 0, 1.0f, 0 };
struct model fox;
struct chunk chunk;

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
	glutInitDisplayMode(GLUT_DEPTH | GLUT_RGBA);
	glutInitWindowSize(w, h);
	glutCreateWindow("Neverdark2 client");
}

void
viewport_init(int nw, int nh) 
{
	vec3 lookat, up;

	w = nw;
	h = nh;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0f, (float) w / (float) h, 0.1f, 1000.0f);
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
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

	/* glutSolidTeapot(.5); */
	glCallList(fox.dl);
	glCallList(chunk.dl);
	chunks_render();

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
key_ldt(double *ldt, char key, long long unsigned tick, double dt)
{
	if (keymap[(unsigned char) key]) {
		*ldt = (keymap[(unsigned char) key] > ltick)
			? (tick - keymap[(unsigned char) key]) * 0.001
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

long long unsigned
tick_get()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return
		(unsigned long long)(tv.tv_sec) * 1000 +
		(unsigned long long)(tv.tv_usec) / 1000;
}

float cam_speed = 6.0;
float cam_aspeed = 0.8;

void
update() {
	long long unsigned tick = tick_get();
	double dt = (tick - ltick) * 0.001, ldt;

	/* warn("dt %d - %d = %f\n", tick, ltick, dt); */

	if (key_ldt(&ldt, 'd', tick, dt))
		cam_translate(cam_speed * ldt, 0, 0);

	if (key_ldt(&ldt, 'D', tick, dt))
		cam_rotate(-cam_aspeed * ldt, 0.0f, 1.0f, 0.0f);

	if (key_ldt(&ldt, 'a', tick, dt))
		cam_translate(-cam_speed * ldt, 0, 0);

	if (key_ldt(&ldt, 'A', tick, dt))
		cam_rotate(cam_aspeed * ldt, 0.0f, 1.0f, 0.0f);

	if (key_ldt(&ldt, 'w', tick, dt))
		cam_translate(0, 0, -cam_speed * ldt);

	if (key_ldt(&ldt, 'W', tick, dt))
		cam_rotate(cam_aspeed * ldt, 1.0f, 0.0f, 0.0f);

	if (key_ldt(&ldt, 's', tick, dt))
		cam_translate(0, 0, cam_speed * ldt);

	if (key_ldt(&ldt, 'S', tick, dt))
		cam_rotate(-cam_aspeed * ldt, 1.0f, 0.0f, 0.0f);

	if (key_ldt(&ldt, 'q', tick, dt))
		cam_translate(0, cam_speed * ldt, 0);

	if (key_ldt(&ldt, 'Q', tick, dt))
		cam_rotate(cam_aspeed * ldt, 0.0f, 0.0f, 1.0f);

	if (key_ldt(&ldt, 'e', tick, dt))
		cam_translate(0, -cam_speed * ldt, 0);

	if (key_ldt(&ldt, 'E', tick, dt))
		cam_rotate(-cam_aspeed * ldt, 0.0f, 0.0f, 1.0f);

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
		cam_rotate(-2.0f * dx / (float) w, 0.0f, 1.0f, 0.0f);
		cam_rotate(-2.0f * dy / (float) h, 1.0f, 0.0f, 0.0f);
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
	keymap[key] = tick_get();
	if (!keymap[key])
		keymap[key] = 1;
	if (isupper(key))
		keymap[tolower(key)] = 0;
	else if (islower(key))
		keymap[toupper(key)] = 0;
}

int
main(int argc, char *argv[])
{
	vec4 light_pos = { 0.0, 10.0, 0.0, 0.0 };
	/* sleep(1); */
	sock_init();
	gl_init(argc, argv);
	cam_init();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	CBUG(model_load(&fox, "fox2.glb"));
	chunks_init();
	glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

	warn("OpenGL version: %s\n", glGetString(GL_VERSION));
	text_send("auth One=qovmjbl");
	ltick = tick_get();

	glutDisplayFunc(display);
	glutReshapeFunc(resize);
	glutTimerFunc(60, update, 0);
	glutPassiveMotionFunc(mouse_pmove);
	glutKeyboardUpFunc(key_up);
	glutKeyboardFunc(key_down);
	viewport_init(w, h);
	glutMainLoop();

	close(sockfd);

	return 0;
}
