#define NOISE_IMPLEMENTATION
#include "noise.h"
#include "chunks.h"
#include <GL/freeglut.h>
#define SDB_IMPLEMENTATION
#include "sdb.h"

#define M2F(mi) (200.0 * (double) mi / (double) NOISE_MAX)

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
chunk_load(struct chunk *chunk, int16_t *s) {
	static octave_t oct[] = \
		{{ 10, 1 }, { 8, 2 }, { 6, 3 }, { 5, 4 }, { 4, 5 }, { 3, 6 }, { 2, 7 }, { 1, 8 }};

	noise_t m[CHUNK_M];
	memcpy(chunk->pos, s, CHUNK_DIM * sizeof(int16_t));

	noise_oct(m, s, sizeof(oct) / sizeof(octave_t), oct, 0, CHUNK_Y, 2);
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
	int16_t c2s[4] = { -CHUNK_SIZE, -CHUNK_SIZE, 0, 0 },
		c3s[4] = { 0, 0, 0, 0 },
		c4s[4] = { -CHUNK_SIZE, 0, 0, 0 },
		c5s[4] = { 0, -CHUNK_SIZE, 0, 0 };
	struct chunk c2, c3, c4, c5;

	sdb_init(&chunks_sdb, 4, sizeof(struct chunk), NULL, NULL);

	chunk_load(&c2, c2s);
	sdb_put(&chunks_sdb, c2s, &c2, 0);

	chunk_load(&c3, c3s);
	sdb_put(&chunks_sdb, c3s, &c3, 0);

	chunk_load(&c4, c4s);
	sdb_put(&chunks_sdb, c4s, &c4, 0);

	chunk_load(&c5, c5s);
	sdb_put(&chunks_sdb, c5s, &c5, 0);
}

void
chunk_render(int16_t *pv, void *ptr) {
	struct chunk *chunk = ptr;
	glPushMatrix();
	glTranslatef(chunk->pos[0], 0, chunk->pos[1]);
	glCallList(chunk->dl);
	glPopMatrix();
}

void chunks_render() {
	int16_t min[4] = { - CHUNK_SIZE * 2, - CHUNK_SIZE * 2, 0, 0 };
	int16_t max[4] = { CHUNK_SIZE * 2, CHUNK_SIZE * 2, 0, 0 };
	chunks_sdb.callback = &chunk_render;
	sdb_search(&chunks_sdb, min, max);
}
