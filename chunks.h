#ifndef CHUNKS_H
#define CHUNKS_H

#include <cglm/cglm.h>
#include "noise.h"
#include "gl_global.h"

#define CHUNK_Y 7
#define CHUNK_SIZE (1 << CHUNK_Y)
#define CHUNK_M (CHUNK_SIZE * CHUNK_SIZE)
#define CHUNK_DIM 3
#ifdef CHUNK_TREE
#define CHUNK_TYPE DB_BTREE
#else
#define CHUNK_TYPE DB_HASH
#endif

struct chunk {
	// key
	int16_t pos[4];
	// value
	noise_t m[CHUNK_M];
	vec3 vertices[CHUNK_M];
	vec3 normals[CHUNK_M];
	cgl_t gl[9];
};

void chunks_init();
void chunks_render();

enum chunk_dl {
	CDL_NXNY,
	CDL_NY,
	CDL_XNY,
	CDL_NX,
	CDL_CENTER,
	CDL_X,
	CDL_NXY,
	CDL_Y,
	CDL_XY,
};

#endif
