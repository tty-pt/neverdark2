#include <arpa/inet.h>
#include <cglm/cglm.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
/* #include <ode/ode.h> */
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "model.h"
#include "shared.h"
#include "chunks.h"
#include "debug.h"
#include "gl_global.h"

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
mat4 viewm = {
	{ 1.0f, 0, 0, 0 },
	{ 0, 1.0f, 0, 0 },
	{ 0, 0, 1.0f, 0 },
	{ 0, 0, 0, 1.0f },
};
mat4 projm = {
	{ 1.0f, 0, 0, 0 },
	{ 0, 1.0f, 0, 0 },
	{ 0, 0, 1.0f, 0 },
	{ 0, 0, 0, 1.0f },
};
struct model fox;
unsigned sprog;
unsigned colorLoc;
unsigned viewLoc, projectionLoc;
unsigned lightPosLoc, viewPosLoc, lightColorLoc, objectColorLoc, objectTextureLoc;

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
	glm_perspective(glm_rad(45.0f), (float) w / (float) h, 0.1f, 1000.0f, projm);
	glUniformMatrix4fv(projectionLoc, 1, 0, &projm[0][0]);

	glm_quat_rotatev(cam.rotation, frontv, lookat);
	glm_quat_rotatev(cam.rotation, upv, up);
	glm_vec3_add(cam.position, lookat, lookat);
	glm_lookat(cam.position, lookat, up, viewm);
	glUniformMatrix4fv(viewLoc, 1, 0, &viewm[0][0]);
	gluLookAt(cam.position[0], cam.position[1], cam.position[2],
		  lookat[0], lookat[1], lookat[2],
		  up[0], up[1], up[2]);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	/* warn("cam p(%f,%f,%f) la(%f,%f,%f) u(%f,%f,%f)\n", */
	/*      cam.position[0], cam.position[1], cam.position[2], */
	/*      lookat[0], lookat[1], lookat[2], */
	/*      up[0], up[1], up[2]); */
}

struct bmesh {
	float *points;
	float *colors;
	float *normals;
	unsigned vao;
	unsigned npoints;
} tri;

static void
bmesh_init(struct bmesh *bmesh)
{
	unsigned points_vbo, normals_vbo;
	glGenVertexArrays(1, &bmesh->vao);

	glGenBuffers(1, &points_vbo);
	glGenBuffers(1, &normals_vbo);
	glBindVertexArray(bmesh->vao);

	glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
	glBufferData(GL_ARRAY_BUFFER, bmesh->npoints * 3 * sizeof(float), bmesh->points, GL_STATIC_DRAW);
	glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glBindBuffer(GL_ARRAY_BUFFER, normals_vbo);
	glBufferData(GL_ARRAY_BUFFER, bmesh->npoints * 3 * sizeof(float), bmesh->normals, GL_STATIC_DRAW);
	glVertexAttribPointer(normalLoc, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(positionLoc);
	glEnableVertexAttribArray(normalLoc);
	GL_DEBUG("bmesh_init");
}

static void
bmesh_render(struct bmesh *bmesh)
{
	glm_mat4_identity(modelm);
	glUniformMatrix4fv(modelLoc, 1, 0, &modelm[0][0]);
	glBindVertexArray(bmesh->vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	GL_DEBUG("bmesh_render");
}

void
display() {
	/* float projMatrix[16], viewMatrix[16]; */

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	/* glUseProgram(sprog); */
	/* glGetFloatv(GL_PROJECTION_MATRIX, projMatrix); */
	/* glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix); */
	/* glUniformMatrix4fv(projMatrixLoc, 1, 0, projMatrix); */
	/* glUniformMatrix4fv(viewMatrixLoc, 1, 0, viewMatrix); */

	/* glutSolidTeapot(.5); */
	model_render(&fox);
	GL_DEBUG("before chunks_render");
	chunks_render();
	GL_DEBUG("before bmesh_render");
	bmesh_render(&tri);

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

static unsigned
shader_load(char *fname, int type)
{
	unsigned shader;
	int success, fd = open(fname, O_RDONLY);
	struct stat stat;
	char *source;

	CBUG(fd < 0);

	success = !fstat(fd, &stat);
	CBUG(!success);

	source = mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	CBUG(!source);

	shader = glCreateShader(type);
	glShaderSource(shader, 1, (const char * const *) &source, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (!success) {
		char infolog[512];

		glGetShaderInfoLog(shader, sizeof(infolog), NULL, infolog);
		BUG("Error compiling shader %s: %s", fname, infolog);
	}

	munmap(source, stat.st_size);
	return shader;
}

unsigned
sprog_load() {
	unsigned vert = shader_load("test.vert.glsl", GL_VERTEX_SHADER);
	unsigned frag = shader_load("test.frag.glsl", GL_FRAGMENT_SHADER);
	unsigned prog = glCreateProgram();
	int success;

	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	/* glBindAttribLocation(prog, 0, "position"); */
	/* glBindFragDataLocation(prog, 0, "fragColor"); */
	glLinkProgram(prog);

	glGetProgramiv(prog, GL_LINK_STATUS, &success);

	if (!success) {
		char infolog[512];
		glGetProgramInfoLog(prog, sizeof(infolog), NULL, infolog);
		BUG("Error linking sprog: %s", infolog);
	}

	positionLoc = glGetAttribLocation(prog, "coord3d");
	normalLoc = glGetAttribLocation(prog, "aNormal");
	texCoordLoc = glGetAttribLocation(prog, "aTexCoord");
	modelLoc = glGetUniformLocation(prog, "model");
	viewLoc = glGetUniformLocation(prog, "view");
	projectionLoc = glGetUniformLocation(prog, "projection");

	lightPosLoc = glGetUniformLocation(prog, "lightPos");
	viewPosLoc = glGetUniformLocation(prog, "viewPos");
	lightColorLoc = glGetUniformLocation(prog, "lightColor");
	objectColorLoc = glGetUniformLocation(prog, "objectColor");

	objectTextureLoc = glGetUniformLocation(prog, "objectTexture");

	CBUG(glGetError());
	CBUG(positionLoc < 0);

	glUseProgram(prog);
	glDeleteShader(vert);
	glDeleteShader(frag);
	CBUG(glGetError());

	glUniform1i(objectTextureLoc, 0);
	/* vertexLoc = glGetAttribLocation(prog, "position"); */
	/* colorLoc = glGetAttribLocation(prog, "color"); */

	/* projMatrixLoc = glGetUniformLocation(prog, "projMatrix"); */
	/* viewMatrixLoc = glGetUniformLocation(prog, "viewMatrix"); */
	return prog;
}

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

static void
tri_init()
{
	tri.npoints = 3;
	tri.points = malloc(sizeof(float) * tri.npoints * 3);
	tri.normals = malloc(sizeof(float) * tri.npoints * 3);
	tri.colors = malloc(sizeof(float) * tri.npoints * 3);

	tri.points[0] = -0.5;
	tri.points[1] = -0.5;
	tri.points[2] = 0;
	tri.normals[0] = 0.0;
	tri.normals[1] = 0.0;
	tri.normals[2] = -1.0;

	tri.points[3] = 0.5;
	tri.points[4] = -0.5;
	tri.points[5] = 0;
	tri.normals[3] = 0.0;
	tri.normals[4] = 0.0;
	tri.normals[5] = -1.0;

	tri.points[6] = 0;
	tri.points[7] = 0.5;
	tri.points[8] = 0;
	tri.normals[6] = 0.0;
	tri.normals[7] = 0.0;
	tri.normals[8] = -1.0;

	tri.colors[0] = tri.colors[1] = tri.colors[2]
		= tri.colors[3] = tri.colors[4] = tri.colors[5]
		= tri.colors[6] = tri.colors[7] = tri.colors[8] = 1.0f;

	bmesh_init(&tri);
}

int
main(int argc, char *argv[])
{
	vec4 light_pos = { 0.0, 400.0, 0.0, 0.0 };
	/* sleep(1); */
	sock_init();
	gl_init(argc, argv);
	glewInit();
	glg_init();
	sprog = sprog_load();
	glm_mat4_identity(modelm);
	glUniformMatrix4fv(modelLoc, 1, 0, &modelm[0][0]);
	glUniform3fv(lightPosLoc, 1, light_pos);
	glUniform3fv(viewPosLoc, 1, cam.position);
	vec3 light_color = { 1.0f, 1.0f, 1.0f };
	glUniform3fv(lightColorLoc, 1, light_color);
	vec3 object_color = { 1.0f, 1.0f, 1.0f };
	glUniform3fv(objectColorLoc, 1, object_color);

	cam_init();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glActiveTexture(GL_TEXTURE0);
	CBUG(model_load(&fox, "fox2.glb"));
	chunks_init();
	tri_init();
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
