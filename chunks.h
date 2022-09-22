#ifndef CHUNKS_H
#define CHUNKS_H

#include <cglm/cglm.h>
#include "noise.h"

#define CHUNK_Y 7
#define CHUNK_SIZE (1 << CHUNK_Y)
#define CHUNK_M (CHUNK_SIZE * CHUNK_SIZE)
#define CHUNK_DIM 4

struct chunk {
	int16_t pos[CHUNK_DIM];
	noise_t m[CHUNK_M];
	vec3 vertices[CHUNK_M];
	vec3 normals[CHUNK_M];
	int dl;
};

void chunk_load(struct chunk *, int16_t *s);
void chunks_init();
void chunks_render();

#endif
