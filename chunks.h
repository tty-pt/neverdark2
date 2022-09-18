#ifndef CHUNKS_H
#define CHUNKS_H

#include <cglm/cglm.h>
#include "noise.h"

#define CHUNK_Y 7
#define CHUNK_SIZE (1 << CHUNK_Y)
#define CHUNK_M (CHUNK_SIZE * CHUNK_SIZE)

struct chunk {
	noise_t m[CHUNK_M];
	vec3 vertices[CHUNK_M];
	vec3 normals[CHUNK_M];
	int dl;
};

void chunk_load(struct chunk *);
void chunks_init();

#endif
