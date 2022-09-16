#include "chunks.h"
#include <cglm/cglm.h>
#include <GL/freeglut.h>
#include "noise.h"

NOISE(2);

#define CHUNK_Y 7
#define CHUNK_SIZE (1 << CHUNK_Y)
#define CHUNK_M (CHUNK_SIZE * CHUNK_SIZE)

int chunk_load() {
	static octave_t oct[] = \
		{{ 7, 2 }, { 6, 3 }, { 5, 4 }, { 4, 5 }, { 3, 6 }, { 2, 7 }};

	noise_t m[CHUNK_M];
	vec3 vertices[CHUNK_M];
	int16_t s[2] = { 0, 0 };
	int dl;

	noise_oct2(m, s, sizeof(oct) / sizeof(octave_t), oct, 0, CHUNK_Y);

	for (int i = 0; i < 1 << CHUNK_Y; i++)
		for (int j = 0; j < 1 << CHUNK_Y; j++) {
			int idx = i * (1 << CHUNK_Y) + j;
			vertices[idx][0] = (double) i;
			vertices[idx][1] = 300 * ((double) m[idx] / (double) NOISE_MAX);
			vertices[idx][2] = (double) j;
		}

	dl = glGenLists(1);
	glNewList(dl, GL_COMPILE);
	glDisable(GL_TEXTURE_2D);

	for (int i = 0; i < (1 << CHUNK_Y) - 1; i++) {
		glBegin(GL_TRIANGLE_STRIP);
		for (int j = 0; j < (1 << CHUNK_Y) - 1; j++) {
			int idx = i * (1 << CHUNK_Y) + j;
			glVertex3fv(vertices[idx]);
			glVertex3fv(vertices[idx + 1]);
			glVertex3fv(vertices[idx + (1 << CHUNK_Y)]);
			glVertex3fv(vertices[idx + (1 << CHUNK_Y) + 1]);
	   	}
		glEnd();
	}

	glEnable(GL_TEXTURE_2D);
	glEndList();

	return dl;
}
