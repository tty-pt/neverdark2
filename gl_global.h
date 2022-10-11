#ifndef GL_GLOBAL_H
#define GL_GLOBAL_H

#include <cglm/cglm.h>
#include <glad/gles2.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define TEX_INVALID ((unsigned) -1)

struct texture {
	char *data;
	int w, h, channels;
};

typedef struct {
	unsigned vao, points_vbo, normals_vbo, tex_coords_vbo;
	unsigned npoints, updated;
} cgl_t;

extern mat4 modelm;
extern unsigned modelLoc;
extern unsigned positionLoc, normalLoc, texCoordLoc;
extern unsigned white_texture;
extern GLFWwindow *glfw_window;

static inline void
cgl_prebind(cgl_t *cgl, int flags)
{
	glGenVertexArrays(1, &cgl->vao);
	glGenBuffers(1, &cgl->points_vbo);
	glGenBuffers(1, &cgl->normals_vbo);
	if (flags & 1)
		glGenBuffers(1, &cgl->tex_coords_vbo);
}

static inline void
cgl_bind(cgl_t *cgl, vec3 *points, vec3 *normals, vec2 *tex_coords)
{
	glBindVertexArray(cgl->vao);

	glBindBuffer(GL_ARRAY_BUFFER, cgl->points_vbo);
	glBufferData(GL_ARRAY_BUFFER, cgl->npoints * 3 * sizeof(float), points, GL_STATIC_DRAW);
	glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glBindBuffer(GL_ARRAY_BUFFER, cgl->normals_vbo);
	glBufferData(GL_ARRAY_BUFFER, cgl->npoints * 3 * sizeof(float), normals, GL_STATIC_DRAW);
	glVertexAttribPointer(normalLoc, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(positionLoc);
	glEnableVertexAttribArray(normalLoc);

	if (tex_coords) {
		glBindBuffer(GL_ARRAY_BUFFER, cgl->tex_coords_vbo);
		glBufferData(GL_ARRAY_BUFFER, cgl->npoints * 2 * sizeof(float), tex_coords, GL_STATIC_DRAW);
		glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(texCoordLoc);
	}

}

void glg_init();
void glg_destroy();
unsigned glg_texture_load(struct texture *tex);

#endif
