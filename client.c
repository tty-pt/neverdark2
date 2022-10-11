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
double lx = -1, ly = -1;
double ltick;
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

void
viewport_init(int nw, int nh) 
{
	vec3 lookat, up;

	w = nw;
	h = nh;
	glm_perspective(glm_rad(45.0f), (float) w / (float) h, 0.1f, 1000.0f, projm);
	glUniformMatrix4fv(projectionLoc, 1, 0, &projm[0][0]);

	glm_quat_rotatev(cam.rotation, frontv, lookat);
	glm_quat_rotatev(cam.rotation, upv, up);
	glm_vec3_add(cam.position, lookat, lookat);
	glm_lookat(cam.position, lookat, up, viewm);
	glUniformMatrix4fv(viewLoc, 1, 0, &viewm[0][0]);

	/* warn("cam p(%f,%f,%f) la(%f,%f,%f) u(%f,%f,%f)\n", */
	/*      cam.position[0], cam.position[1], cam.position[2], */
	/*      lookat[0], lookat[1], lookat[2], */
	/*      up[0], up[1], up[2]); */
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
	return prog;
}

void
cam_init() {
	cam.position[0] = 0;
	cam.position[1] = 0;
	cam.position[2] = 0;
	glm_quat(cam.rotation, glm_deg(0.0f), 0.0f, 1.0f, 0.0f);
}

static void
error_callback(int error, const char *description)
{
	warn("GLFW: %d %s\n", error, description);

}

int
main(int argc, char *argv[])
{
	vec4 light_pos = { 0.0, 400.0, 0.0, 0.0 };
	/* sleep(1); */
	sock_init();
	glg_init(w, h);
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
	/* glActiveTexture(GL_TEXTURE0); */
	CBUG(model_load(&fox, "fox2.glb"));
	chunks_init();

	warn("OpenGL version: %s\n", glGetString(GL_VERSION));
	text_send("auth One=qovmjbl");
	ltick = glfwGetTime();

	glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glfwSetErrorCallback(error_callback);

	while (!glfwWindowShouldClose(glfw_window)) {
		double tick = glfwGetTime();
		double dt = tick - ltick;
		int nw, nh;
		ltick = tick;

		glfwGetFramebufferSize(glfw_window, &nw, &nh);
		if (nw != w || nh != h) {
			w = nw;
			h = nh;
			updated_cam = 1;
		}

		{
			double x, y;
			glfwGetCursorPos(glfw_window, &x, &y);
			if (lx != -1) {
				double dx = x - lx;
				double dy = y - ly;
				cam_rotate(-2.0f * dx / w, 0.0f, 1.0f, 0.0f);
				cam_rotate(-2.0f * dy / h, 1.0f, 0.0f, 0.0f);
			}
			lx = x;
			ly = y;
		}

		if (glfwGetKey(glfw_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
		    || glfwGetKey(glfw_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
			if (glfwGetKey(glfw_window, GLFW_KEY_W) == GLFW_PRESS)
				cam_rotate(cam_aspeed * dt, 1.0f, 0.0f, 0.0f);

			if (glfwGetKey(glfw_window, GLFW_KEY_S) == GLFW_PRESS)
				cam_rotate(-cam_aspeed * dt, 1.0f, 0.0f, 0.0f);

			if (glfwGetKey(glfw_window, GLFW_KEY_A) == GLFW_PRESS)
				cam_rotate(cam_aspeed * dt, 0.0f, 1.0f, 0.0f);

			if (glfwGetKey(glfw_window, GLFW_KEY_D) == GLFW_PRESS)
				cam_rotate(-cam_aspeed * dt, 0.0f, 1.0f, 0.0f);

			if (glfwGetKey(glfw_window, GLFW_KEY_Q) == GLFW_PRESS)
				cam_rotate(cam_aspeed * dt, 0.0f, 0.0f, 1.0f);

			if (glfwGetKey(glfw_window, GLFW_KEY_E) == GLFW_PRESS)
				cam_rotate(-cam_aspeed * dt, 0.0f, 0.0f, 1.0f);
		} else {
			if (glfwGetKey(glfw_window, GLFW_KEY_W) == GLFW_PRESS)
				cam_translate(0, 0, -cam_speed * dt);

			if (glfwGetKey(glfw_window, GLFW_KEY_S) == GLFW_PRESS)
				cam_translate(0, 0, cam_speed * dt);

			if (glfwGetKey(glfw_window, GLFW_KEY_A) == GLFW_PRESS)
				cam_translate(-cam_speed * dt, 0, 0);

			if (glfwGetKey(glfw_window, GLFW_KEY_D) == GLFW_PRESS)
				cam_translate(cam_speed * dt, 0, 0);

			if (glfwGetKey(glfw_window, GLFW_KEY_Q) == GLFW_PRESS)
				cam_translate(0, cam_speed * dt, 0);

			if (glfwGetKey(glfw_window, GLFW_KEY_E) == GLFW_PRESS)
				cam_translate(0, -cam_speed * dt, 0);
		}

		if (updated_cam) {
			glViewport(0, 0, w, h);
			viewport_init(w, h);
			updated_cam = 0;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		model_render(&fox);
		chunks_render();

		glfwSwapBuffers(glfw_window);
		glfwPollEvents();
	}

	glg_destroy();
	close(sockfd);

	return 0;
}
