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
}

static inline void
chunk_vertex(struct chunk *chunk, int idx)
{
	glNormal3fv(chunk->normals[idx]);
	glVertex3fv(chunk->vertices[idx]);
}

struct chunk
chunk_load(int16_t *s) {
	point_debug(s, "chunk_load", CHUNK_DIM);
	struct chunk chunk;
	static octave_t oct[] = \
		{{ 10, 1 }, { 8, 2 }, { 6, 3 }, { 5, 4 }, { 4, 5 }, { 3, 6 }, { 2, 7 }, { 1, 8 }};

	noise_t m[CHUNK_M];
	memcpy(chunk.pos, s, CHUNK_DIM * sizeof(int16_t));

	noise_oct(m, s, sizeof(oct) / sizeof(octave_t), oct, 0, CHUNK_Y, 2);
	memcpy(chunk.m, m, CHUNK_M * sizeof(noise_t));

	for (int i = 0; i < 1 << CHUNK_Y; i++)
		for (int j = 0; j < 1 << CHUNK_Y; j++) {
			int idx = i * (1 << CHUNK_Y) + j;
			chunk.vertices[idx][0] = (double) i;
			chunk.vertices[idx][1] = M2F(chunk.m[idx]);
			chunk.vertices[idx][2] = (double) j;
			chunk_normal(&chunk, idx);
		}

	chunk.dl = glGenLists(1);
	glNewList(chunk.dl, GL_COMPILE);
	glDisable(GL_TEXTURE_2D);

	for (int i = 0; i < CHUNK_SIZE - 1; i++) {
		int idx = i * CHUNK_SIZE;
		glBegin(GL_TRIANGLE_STRIP);
		chunk_vertex(&chunk, idx);
		chunk_vertex(&chunk, idx + CHUNK_SIZE);
		for (int j = 0; j < CHUNK_SIZE - 1; j++) {
			chunk_vertex(&chunk, idx + j + 1);
			chunk_vertex(&chunk, idx + j + CHUNK_SIZE + 1);
	   	}
		glEnd();
	}

	glEnable(GL_TEXTURE_2D);
	glEndList();

	sdb_put(&chunks_sdb, s, &chunk, 0);
	return chunk;
}

void chunks_init() {
	sdb_init(&chunks_sdb, 3, CHUNK_Y, sizeof(struct chunk), NULL, NULL, DB_HASH);
}

void
chunk_render(int16_t *pv, void *ptr) {
	struct chunk *chunk;

	if (!ptr) {
		chunk_load(pv);
		return;
	}

	chunk = ptr;
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
