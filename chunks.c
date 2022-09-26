#define NOISE_IMPLEMENTATION
#include "noise.h"
#include "chunks.h"
#include <GL/freeglut.h>
#define SDB_IMPLEMENTATION
#include "sdb.h"

#define M2F(mi) (200.0 * (double) mi / (double) NOISE_MAX)

sdb_t chunks_sdb;

float mat_red[3] = { 1.0, 0.0, 0.0 };
float mat_green[3] = { 0.0, 1.0, 0.0 };
float mat_blue[3] = { 0.0, 0.0, 1.0 };
float mat_yellow[3] = { 1.0, 1.0, 0.0 };
float mat_cyan[3] = { 0.0, 1.0, 1.0 };
float mat_purple[3] = { 1.0, 0.0, 1.0 };
float mat_white[3] = { 1.0, 1.0, 1.0 };

static inline void
normal_fix(struct chunk *chunk, int idx, noise_t hl, noise_t hu, noise_t hr, noise_t hd)
{
	chunk->normals[idx][0] = M2F(hr) - M2F(hl);
	chunk->normals[idx][1] = 2.0f;
	chunk->normals[idx][2] = M2F(hd) - M2F(hu);
	glm_vec3_normalize(chunk->normals[idx]);
}

static inline void
chunk_normal(struct chunk *chunk, int idx)
{
	noise_t hh = chunk->m[idx];
	normal_fix(chunk, idx,
		   idx - CHUNK_SIZE >= 0 ? chunk->m[idx - CHUNK_SIZE] : hh,
		   idx - 1 >= 0 ? chunk->m[idx - 1] : hh,
		   idx + CHUNK_SIZE < CHUNK_M ? chunk->m[idx + CHUNK_SIZE] : hh,
		   idx + 1 < CHUNK_M ? chunk->m[idx + 1] : hh);
}

static inline void
chunk_vertex(struct chunk *chunk, int idx)
{
	glNormal3fv(chunk->normals[idx]);
	glVertex3fv(chunk->vertices[idx]);
}

static void
chunk_fix_x(struct chunk *chunk, struct chunk *chunkx)
{
	int i = CHUNK_SIZE - 1;
	int idx = i * CHUNK_SIZE;

	// i is x, j is y
	for (int j = 0; j < CHUNK_SIZE; j++) {
		noise_t hh = chunk->m[idx + j];
		normal_fix(chunk, idx + j,
			   chunk->m[idx + j - CHUNK_SIZE],
			   idx + j - 1 >= 0 ? chunk->m[idx + j - 1] : hh,
			   chunkx->m[j],
			   idx + j + 1 < CHUNK_M ? chunk->m[idx + j + 1] : hh);

		idx = j;
		hh = chunkx->m[idx];

		normal_fix(chunkx, idx,
			   chunk->m[idx + (CHUNK_SIZE - 1) * CHUNK_SIZE],
			   idx - 1 >= 0 ? chunkx->m[idx - 1] : hh,
			   chunkx->m[idx + CHUNK_SIZE],
			   idx + 1 < CHUNK_M ? chunkx->m[idx + 1] : hh);
	}

	glDeleteLists(chunk->dl[CDL_X], 1);
	chunk->dl[CDL_X] = glGenLists(1);
	glNewList(chunk->dl[CDL_X], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_blue); */

	// i is x, j is y
	{
		int i = CHUNK_SIZE - 2, j;
		glBegin(GL_TRIANGLE_STRIP);
		for (j = 1; j < CHUNK_SIZE - 1; j++) {
			int idx = i * CHUNK_SIZE + j;
			chunk_vertex(chunk, idx);
			chunk_vertex(chunk, idx + CHUNK_SIZE);
		}
		glEnd();

		j = 0;
		i++;
		idx = i * CHUNK_SIZE;
		glBegin(GL_TRIANGLE_STRIP);
		for (j = 1; j < CHUNK_SIZE - 1; j++) {
			chunk_vertex(chunk, idx + j);
			glNormal3fv(chunkx->normals[j]);
			glVertex3f(chunk->vertices[idx][0] + 1,
				   M2F(chunkx->m[j]),
				   chunk->vertices[idx][2] + j);
		}
		glEnd();
	}

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

	chunkx->dl[CDL_NX] = glGenLists(1);
	glNewList(chunkx->dl[CDL_NX], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_green); */

	// x is 0, j is y
	{
		glBegin(GL_TRIANGLE_STRIP);
		for (int j = 1; j < CHUNK_SIZE - 1; j++) {
			chunk_vertex(chunkx, j);
			chunk_vertex(chunkx, j + CHUNK_SIZE);
		}
		glEnd();
	}

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

}

static void
chunk_fix_y(struct chunk *chunk, struct chunk *chunky)
{
	int i, j = CHUNK_SIZE - 1, idx;
	noise_t hh;

	// i is x, j is y
	for (i = 1; i < CHUNK_SIZE - 1; i++) {
		j = CHUNK_SIZE - 1;
		idx = i * CHUNK_SIZE + j;
		normal_fix(chunk, idx,
			   chunk->m[idx - CHUNK_SIZE],
			   chunk->m[idx - 1],
			   chunk->m[idx + CHUNK_SIZE],
			   chunky->m[idx - j + 1]);

		idx -= j;
		normal_fix(chunky, idx,
			   chunky->m[idx - CHUNK_SIZE],
			   chunk->m[idx + j - 1],
			   chunky->m[idx + CHUNK_SIZE],
			   chunky->m[idx + 1]);
	}

	j = 0;
	i = CHUNK_SIZE - 1;
	idx = CHUNK_SIZE * i + j;
	hh = chunky->m[idx];
	normal_fix(chunky, idx,
		   chunky->m[idx - CHUNK_SIZE],
		   chunk->m[idx + CHUNK_SIZE - 1],
		   hh,
		   chunky->m[idx + 1]);

	j = CHUNK_SIZE - 1;
	i = CHUNK_SIZE - 1;
	idx = CHUNK_SIZE * i + j;
	hh = chunk->m[idx];
	normal_fix(chunk, idx,
		   chunk->m[idx - CHUNK_SIZE],
		   chunk->m[idx - 1],
		   hh,
		   chunky->m[idx - j]);

	glDeleteLists(chunky->dl[CDL_NY], 1);
	chunky->dl[CDL_NY] = glGenLists(1);
	glNewList(chunky->dl[CDL_NY], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_yellow); */

	/* // i is x and j is y */
	glBegin(GL_TRIANGLE_STRIP);
	for (i = 1; i < CHUNK_SIZE - 1; i++) {
		int idx = i * CHUNK_SIZE;
		chunk_vertex(chunky, idx);
		chunk_vertex(chunky, idx + 1);
	}
	glEnd();

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

	glDeleteLists(chunk->dl[CDL_Y], 1);
	chunk->dl[CDL_Y] = glGenLists(1);
	glNewList(chunk->dl[CDL_Y], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_cyan); */

	// i is x and j is y
	j = CHUNK_SIZE - 2;
	glBegin(GL_TRIANGLE_STRIP);
	for (int i = 1; i < CHUNK_SIZE - 1; i++) {
		int idx = i * CHUNK_SIZE + j;
		chunk_vertex(chunk, idx);
		chunk_vertex(chunk, idx + 1);
	}
	glEnd();

	j = CHUNK_SIZE - 1;
	glBegin(GL_TRIANGLE_STRIP);
	for (int i = 1; i < CHUNK_SIZE - 1; i++) {
		idx = i * CHUNK_SIZE + j;
		chunk_vertex(chunk, idx);
		glNormal3fv(chunky->normals[idx - j]);
		glVertex3f(chunk->vertices[idx][0],
			   M2F(chunky->m[idx - j]),
			   chunk->vertices[idx][2] + 1);
	}
	glEnd();

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

}

static void
chunk_fix_xy(struct chunk *chunk, struct chunk *chunkx, struct chunk *chunky, struct chunk *chunkxy)
{
	int i = 0, j = 0, idx = 0;

	normal_fix(chunkxy, idx,
		   chunky->m[CHUNK_SIZE * (CHUNK_SIZE - 1)],
		   chunkx->m[CHUNK_SIZE - 1],
		   chunkxy->m[CHUNK_SIZE],
		   chunkxy->m[1]);

	i = CHUNK_SIZE - 1;
	j = CHUNK_SIZE - 1;
	idx = CHUNK_SIZE * i + j;
	
	normal_fix(chunk, idx,
		   chunk->m[idx - CHUNK_SIZE],
		   chunk->m[idx - 1],
		   chunkx->m[CHUNK_SIZE - 1],
		   chunky->m[CHUNK_SIZE * i]);

	i = 0;
	j = CHUNK_SIZE - 1;
	idx = CHUNK_SIZE * i + j;

	normal_fix(chunkx, idx,
		   chunk->m[CHUNK_SIZE * (CHUNK_SIZE - 1) + j],
		   chunk->m[idx - 1],
		   chunkx->m[CHUNK_SIZE],
		   chunkxy->m[0]);

	i = CHUNK_SIZE - 1;
	j = 0;
	idx = CHUNK_SIZE * i + j;

	normal_fix(chunky, idx,
		   chunky->m[idx - CHUNK_SIZE],
		   chunk->m[idx + CHUNK_SIZE - 1],
		   chunkxy->m[0],
		   chunky->m[idx + 1]);

	i = 0;
	j = CHUNK_SIZE - 1;
	idx = CHUNK_SIZE * i + j;

	normal_fix(chunkx, idx,
		   chunk->m[CHUNK_SIZE * (CHUNK_SIZE - 1) + CHUNK_SIZE - 1],
		   chunkx->m[CHUNK_SIZE - 2],
		   chunkx->m[CHUNK_SIZE - 1 + CHUNK_SIZE],
		   chunkxy->m[0]);

	glDeleteLists(chunk->dl[CDL_XY], 1);
	chunk->dl[CDL_XY] = glGenLists(1);
	glNewList(chunk->dl[CDL_XY], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);

	{
		int i = CHUNK_SIZE - 1;
		int j = CHUNK_SIZE - 1;
		int idx = i * CHUNK_SIZE + j;

		glBegin(GL_TRIANGLE_STRIP);

		chunk_vertex(chunk, idx - 1 - CHUNK_SIZE);
		chunk_vertex(chunk, idx - 1);
		chunk_vertex(chunk, idx - CHUNK_SIZE);
		chunk_vertex(chunk, idx);
		
		glNormal3fv(chunky->normals[idx - j - CHUNK_SIZE]);
		glVertex3f(chunk->vertices[idx][0] - 1,
			   M2F(chunky->m[idx - j - CHUNK_SIZE]),
			   chunk->vertices[idx][2] + 1);

		glNormal3fv(chunky->normals[idx - j]);
		glVertex3f(chunk->vertices[idx][0],
			   M2F(chunky->m[idx - j]),
			   chunk->vertices[idx][2] + 1);

		glEnd();

		glBegin(GL_TRIANGLE_STRIP);

		chunk_vertex(chunk, idx - 1);
		
		glNormal3fv(chunkx->normals[j - 1]);
		glVertex3f(chunk->vertices[idx][0] + 1,
			   M2F(chunkx->m[j - 1]),
			   chunk->vertices[idx][2] - 1);

		chunk_vertex(chunk, idx);

		glNormal3fv(chunkx->normals[j]);
		glVertex3f(chunk->vertices[idx][0] + 1,
			   M2F(chunkx->m[j]),
			   chunk->vertices[idx][2]);

		glNormal3fv(chunky->normals[idx - j]);
		glVertex3f(chunk->vertices[idx][0],
			   M2F(chunky->m[idx - j]),
			   chunk->vertices[idx][2] + 1);

		glNormal3fv(chunkxy->normals[0]);
		glVertex3f(chunk->vertices[idx][0] + 1,
			   M2F(chunkxy->m[0]),
			   chunk->vertices[idx][2] + 1);

		glEnd();
	}

	glEnable(GL_TEXTURE_2D);
	glEndList();

	glDeleteLists(chunkxy->dl[CDL_NXNY], 1);
	chunkxy->dl[CDL_NXNY] = glGenLists(1);
	glNewList(chunkxy->dl[CDL_NXNY], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);

	{
		glBegin(GL_TRIANGLE_STRIP);

		chunk_vertex(chunkxy, 0);
		chunk_vertex(chunkxy, CHUNK_SIZE);
		chunk_vertex(chunkxy, 1);
		chunk_vertex(chunkxy, CHUNK_SIZE + 1);
		glEnd();
	}

	glEnable(GL_TEXTURE_2D);
	glEndList();

	glDeleteLists(chunky->dl[CDL_XNY], 1);
	chunky->dl[CDL_XNY] = glGenLists(1);
	glNewList(chunky->dl[CDL_XNY], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_TRIANGLE_STRIP);

	i = CHUNK_SIZE - 2;
	j = 0;
	idx = i * CHUNK_SIZE + j;

	chunk_vertex(chunky, idx);
	chunk_vertex(chunky, idx + 1);
	chunk_vertex(chunky, idx + CHUNK_SIZE);
	chunk_vertex(chunky, idx + CHUNK_SIZE + 1);

	glNormal3fv(chunkxy->normals[0]);
	glVertex3f(chunky->vertices[idx][0] + 2,
		   M2F(chunkxy->m[0]),
		   chunky->vertices[idx][2]);

	glNormal3fv(chunkxy->normals[1]);
	glVertex3f(chunky->vertices[idx][0] + 2,
		   M2F(chunkxy->m[1]),
		   chunky->vertices[idx][2] + 1);

	glEnd();
	glEnable(GL_TEXTURE_2D);
	glEndList();

	glDeleteLists(chunkx->dl[CDL_NXY], 1);
	chunkx->dl[CDL_NXY] = glGenLists(1);
	glNewList(chunkx->dl[CDL_NXY], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_TRIANGLE_STRIP);

	idx = CHUNK_SIZE - 2;
	glNormal3fv(chunkx->normals[idx]);
	glVertex3f(chunkx->vertices[idx][0],
		   M2F(chunkx->m[idx]),
		   chunkx->vertices[idx][2]);

	glNormal3fv(chunkx->normals[idx + CHUNK_SIZE]);
	glVertex3f(chunkx->vertices[idx][0] + 1,
		   M2F(chunkx->m[idx + CHUNK_SIZE]),
		   chunkx->vertices[idx][2]);

	idx++;
	glNormal3fv(chunkx->normals[idx]);
	glVertex3f(chunkx->vertices[idx][0],
		   M2F(chunkx->m[idx]),
		   chunkx->vertices[idx][2]);

	glNormal3fv(chunkx->normals[idx + CHUNK_SIZE]);
	glVertex3f(chunkx->vertices[idx][0] + 1,
		   M2F(chunkx->m[idx + CHUNK_SIZE]),
		   chunkx->vertices[idx][2]);

	/* idx++; */
	glNormal3fv(chunkxy->normals[0]);
	glVertex3f(chunkx->vertices[idx][0],
		   M2F(chunkxy->m[0]),
		   chunkx->vertices[idx][2] + 1);

	glNormal3fv(chunkxy->normals[CHUNK_SIZE]);
	glVertex3f(chunkx->vertices[idx][0] + 1,
		   M2F(chunkxy->m[CHUNK_SIZE]),
		   chunkx->vertices[idx][2] + 1);

	glEnd();
	glEnable(GL_TEXTURE_2D);
	glEndList();
}

static void
chunk_replace(struct chunk *new)
{
	/* warn("chunk_replace %p\n", &sdb_delete_at); */
	/* sdb_delete(&chunks_sdb, new); */
	sdb_put(&chunks_sdb, new, 0);
}

struct chunk *
chunk_get(int16_t x, int16_t y, int16_t z)
{
	int16_t s[4] = { x, y, z, 0 };
	return (struct chunk *) sdb_get(&chunks_sdb, s);
}

static void
chunk_fix(struct chunk *chunk)
{
	struct chunk *chunknx_r
		= chunk_get(chunk->pos[0] - CHUNK_SIZE,
			    chunk->pos[1],
			    chunk->pos[2]);

	if (chunknx_r) {
		struct chunk chunknx;
		memcpy(&chunknx, chunknx_r, sizeof(struct chunk));
		chunk_fix_x(&chunknx, chunk);

		struct chunk *chunkny_r
			= chunk_get(chunk->pos[0],
				    chunk->pos[1] - CHUNK_SIZE,
				    chunk->pos[2]);

		if (chunkny_r) {
			struct chunk chunkny;
			memcpy(&chunkny, chunkny_r, sizeof(struct chunk));
			chunk_fix_y(&chunkny, chunk);

			struct chunk *chunknxny_r
				= chunk_get(chunk->pos[0] - CHUNK_SIZE,
					    chunk->pos[1] - CHUNK_SIZE,
					    chunk->pos[2]);


			if (chunknxny_r) {
				struct chunk chunknxny;
				memcpy(&chunknxny, chunknxny_r, sizeof(struct chunk));
				chunk_fix_xy(&chunknxny, &chunkny, &chunknx, chunk);
				chunk_replace(&chunknxny);
			}

			chunk_replace(&chunkny);
		}

		chunk_replace(&chunknx);
	} else {
		struct chunk *chunkny_r
			= chunk_get(chunk->pos[0], chunk->pos[1] - CHUNK_SIZE, chunk->pos[2]);

		if (chunkny_r) {
			struct chunk chunkny;
			memcpy(&chunkny, chunkny_r, sizeof(struct chunk));
			chunk_fix_y(&chunkny, chunk);
			chunk_replace(&chunkny);
		}
	}

	struct chunk *chunkx_r
		= chunk_get(chunk->pos[0] + CHUNK_SIZE,
			    chunk->pos[1],
			    chunk->pos[2]);

	if (chunkx_r) {
		struct chunk chunkx;
		memcpy(&chunkx, chunkx_r, sizeof(struct chunk));
		chunk_fix_x(chunk, &chunkx);

		struct chunk *chunky_r
			= chunk_get(chunk->pos[0],
				    chunk->pos[1] + CHUNK_SIZE,
				    chunk->pos[2]);

		if (chunky_r) {
			struct chunk chunky;
			memcpy(&chunky, chunky_r, sizeof(struct chunk));
			chunk_fix_y(chunk, &chunky);

			struct chunk *chunkxy_r
				= chunk_get(chunk->pos[0] + CHUNK_SIZE,
					    chunk->pos[1] + CHUNK_SIZE,
					    chunk->pos[2]);


			if (chunkxy_r) {
				struct chunk chunkxy;
				memcpy(&chunkxy, chunkxy_r, sizeof(struct chunk));
				chunk_fix_xy(chunk, &chunkx, &chunky, &chunkxy);
				chunk_replace(&chunkxy);
			}

			chunk_replace(&chunky);
		}

		chunk_replace(&chunkx);
	} else {
		struct chunk *chunky_r
			= chunk_get(chunk->pos[0],
				    chunk->pos[1] + CHUNK_SIZE,
				    chunk->pos[2]);

		if (chunky_r) {
			struct chunk chunky;
			memcpy(&chunky, chunky_r, sizeof(struct chunk));
			chunk_fix_y(chunk, &chunky);
			chunk_replace(&chunky);
		}
	}

}

struct chunk
chunk_load(int16_t *s) {
	point_debug(s, "chunk_load", CHUNK_DIM);
	struct chunk chunk;
	static octave_t oct[] = \
		{{ 10, 1 }, { 8, 2 }, { 6, 3 }, { 5, 4 }, { 4, 5 }, { 3, 6 }, { 2, 7 }, { 1, 8 }};

	memcpy(chunk.pos, s, CHUNK_DIM * sizeof(int16_t));
	noise_oct(chunk.m, s, sizeof(oct) / sizeof(octave_t), oct, 0, CHUNK_Y, 2);

	for (int i = 0; i < CHUNK_SIZE; i++)
		for (int j = 0; j < CHUNK_SIZE; j++) {
			int idx = i * CHUNK_SIZE + j;
			chunk.vertices[idx][0] = (double) i;
			chunk.vertices[idx][1] = M2F(chunk.m[idx]);
			chunk.vertices[idx][2] = (double) j;
			chunk_normal(&chunk, idx);
		}

	chunk.dl[CDL_CENTER] = glGenLists(1);
	glNewList(chunk.dl[CDL_CENTER], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_red); */

	for (int i = 1; i < CHUNK_SIZE - 2; i++) {
		glBegin(GL_TRIANGLE_STRIP);
		for (int j = 1; j < CHUNK_SIZE - 1; j++) {
			int idx = i * CHUNK_SIZE + j;
			chunk_vertex(&chunk, idx);
			chunk_vertex(&chunk, idx + CHUNK_SIZE);
	   	}
		glEnd();
	}

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

	chunk.dl[CDL_NX] = glGenLists(1);
	glNewList(chunk.dl[CDL_NX], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_green); */

	// x is 0, j is y
	{
		glBegin(GL_TRIANGLE_STRIP);
		for (int j = 1; j < CHUNK_SIZE - 1; j++) {
			chunk_vertex(&chunk, j);
			chunk_vertex(&chunk, j + CHUNK_SIZE);
		}
		glEnd();
	}

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

	chunk.dl[CDL_X] = glGenLists(1);
	glNewList(chunk.dl[CDL_X], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_blue); */

	// i is x, j is y
	{
		int i = CHUNK_SIZE - 2;
		glBegin(GL_TRIANGLE_STRIP);
		for (int j = 1; j < CHUNK_SIZE - 1; j++) {
			int idx = i * CHUNK_SIZE + j;
			chunk_vertex(&chunk, idx);
			chunk_vertex(&chunk, idx + CHUNK_SIZE);
		}
		glEnd();
	}

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

	chunk.dl[CDL_NY] = glGenLists(1);
	glNewList(chunk.dl[CDL_NY], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_yellow); */

	glBegin(GL_TRIANGLE_STRIP);
	for (int i = 1; i < CHUNK_SIZE - 1; i++) {
		int idx = i * CHUNK_SIZE;
		chunk_vertex(&chunk, idx);
		chunk_vertex(&chunk, idx + 1);
	}
	glEnd();

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

	chunk.dl[CDL_Y] = glGenLists(1);
	glNewList(chunk.dl[CDL_Y], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);
	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_cyan); */

	glBegin(GL_TRIANGLE_STRIP);
	for (int i = 1; i < CHUNK_SIZE - 1; i++) {
		int j = CHUNK_SIZE - 2;
		int idx = i * CHUNK_SIZE + j;
		chunk_vertex(&chunk, idx);
		chunk_vertex(&chunk, idx + 1);
	}
	glEnd();

	/* glMaterialfv(GL_FRONT, GL_AMBIENT, mat_white); */
	glEnable(GL_TEXTURE_2D);
	glEndList();

	/* chunk.dl[CDL_XY] = glGenLists(1); */
	/* glNewList(chunk.dl[CDL_XY], GL_COMPILE); */
	/* glEndList(); */

	chunk.dl[CDL_NXNY] = glGenLists(1);
	glNewList(chunk.dl[CDL_NXNY], GL_COMPILE);
	glDisable(GL_TEXTURE_2D);

	{
		glBegin(GL_TRIANGLE_STRIP);
		chunk_vertex(&chunk, 0);
		chunk_vertex(&chunk, CHUNK_SIZE);
		chunk_vertex(&chunk, 1);
		chunk_vertex(&chunk, CHUNK_SIZE + 1);
		glEnd();
	}

	glEnable(GL_TEXTURE_2D);
	glEndList();

	chunk_fix(&chunk);
	sdb_put(&chunks_sdb, &chunk, DB_NOOVERWRITE);
	return chunk;
}

void chunks_init() {
#ifdef CHUNK_TREE // this is just for testing
	int16_t c2s[4] = { -CHUNK_SIZE, -CHUNK_SIZE, 0, 0 },
		c3s[4] = { 0, 0, 0, 0 },
		c4s[4] = { -CHUNK_SIZE, 0, 0, 0 },
		c5s[4] = { 0, -CHUNK_SIZE, 0, 0 };
#endif
	sdb_init(&chunks_sdb, CHUNK_DIM, CHUNK_Y, 0, sizeof(struct chunk), NULL, NULL, CHUNK_TYPE);
#ifdef CHUNK_TREE
	chunk_load(c2s);
	chunk_load(c3s);
	chunk_load(c4s);
	chunk_load(c5s);
#endif
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
	glCallList(chunk->dl[CDL_CENTER]);
	glCallList(chunk->dl[CDL_NXNY]);
	glCallList(chunk->dl[CDL_XNY]);
	glCallList(chunk->dl[CDL_NX]);
	glCallList(chunk->dl[CDL_X]);
	glCallList(chunk->dl[CDL_NY]);
	glCallList(chunk->dl[CDL_NXY]);
	glCallList(chunk->dl[CDL_Y]);
	glCallList(chunk->dl[CDL_XY]);
	glPopMatrix();
}

void chunks_render() {
	int16_t min[4] = { - CHUNK_SIZE * 2, - CHUNK_SIZE * 2, 0, 0 };
	int16_t max[4] = { CHUNK_SIZE * 2 - 1, CHUNK_SIZE * 2 - 1, 0, 0 };
	chunks_sdb.callback = &chunk_render;
	sdb_search(&chunks_sdb, min, max);
}
