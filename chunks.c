#include "chunks.h"
#include <GL/freeglut.h>
#define SDB_IMPLEMENTATION
#include "sdb.h"

NOISE(2);

#define M2F(mi) (300.0 * (double) mi / (double) NOISE_MAX)

sdb_t chunks_sdb;

static inline void
chunk_normal(struct chunk *chunk, int idx)
{
	int side = CHUNK_SIZE;
	float hh = M2F(chunk->m[idx]);
	float hl = idx - 1 >= 0 ? M2F(chunk->m[idx - 1]) : hh;
	float hr = idx + 1 < side ? M2F(chunk->m[idx + 1]) : hh;
	float hu = idx - side >= 0 ? M2F(chunk->m[idx - side]) : hh;
	float hd = idx + CHUNK_SIZE < CHUNK_SIZE * CHUNK_SIZE
		? M2F(chunk->m[idx + CHUNK_SIZE])
		: hh;
	chunk->normals[idx][0] = hr - hl;
	chunk->normals[idx][1] = 2.0f;
	chunk->normals[idx][2] = hd - hu;
	glm_vec3_normalize(chunk->normals[idx]);
	/* warn("chunk_normal %f %f %f\n", normal[0], normal[1], normal[2]); */
	/* glNormal3fv(normal); */
}

static inline void
chunk_vertex(struct chunk *chunk, int idx)
{
	glNormal3fv(chunk->normals[idx]);
	glVertex3fv(chunk->vertices[idx]);
}

void
chunk_load(struct chunk *chunk) {
	static octave_t oct[] = \
		{{ 8, 1 }, { 7, 2 }, { 6, 3 }, { 5, 4 }, { 4, 5 }, { 3, 6 }, { 2, 7 }, { 1, 8 }};

	int16_t s[2] = { 0, 0 };
	noise_t m[CHUNK_M];

	warn("chunks %p\n", chunk->m);
	noise_oct2(m, s, sizeof(oct) / sizeof(octave_t), oct, 0, CHUNK_Y);
	memcpy(chunk->m, m, CHUNK_M * sizeof(noise_t));

	for (int i = 0; i < 1 << CHUNK_Y; i++)
		for (int j = 0; j < 1 << CHUNK_Y; j++) {
			int idx = i * (1 << CHUNK_Y) + j;
			chunk->vertices[idx][0] = (double) i;
			chunk->vertices[idx][1] = M2F(chunk->m[idx]);
			chunk->vertices[idx][2] = (double) j;
			chunk_normal(chunk, idx);
		}

	chunk->dl = glGenLists(1);
	glNewList(chunk->dl, GL_COMPILE);
	glDisable(GL_TEXTURE_2D);

	for (int i = 0; i < CHUNK_SIZE - 1; i++) {
		int idx = i * CHUNK_SIZE;
		glBegin(GL_TRIANGLE_STRIP);
		chunk_vertex(chunk, idx);
		chunk_vertex(chunk, idx + CHUNK_SIZE);
		for (int j = 0; j < CHUNK_SIZE - 1; j++) {
			chunk_vertex(chunk, idx + j + 1);
			chunk_vertex(chunk, idx + j + CHUNK_SIZE + 1);
	   	}
		glEnd();
	}

	glEnable(GL_TEXTURE_2D);
	glEndList();
}

void chunks_init() {
	sdb_init(&chunks_sdb, 3, sizeof(struct chunk), NULL, NULL);
}
