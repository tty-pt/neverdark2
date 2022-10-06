#define NOISE_IMPLEMENTATION
#include "noise.h"
#include "chunks.h"
#define SDB_IMPLEMENTATION
#include "sdb.h"
#include "gl_global.h"

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
cgl_unbind(cgl_t *cgl)
{
	/* glDeleteBuffers(1, &cgl->points_vbo); */
	/* glDeleteBuffers(1, &cgl->normals_vbo); */
	/* glDeleteVertexArrays(1, &cgl->vao); */
	/* cgl->vao = cgl->points_vbo = cgl->normals_vbo = 0; */
	cgl->updated = 1;
}

static inline void
cgl_bind_3f(vec3 to, float x, float y, float z)
{
	vec3 from = { x, y, z };
	glm_vec3_copy(from, to);
}

static inline void
normal_fix(struct chunk *chunk, int idx, noise_t hl, noise_t hu, noise_t hr, noise_t hd)
{
	chunk->normals[idx][0] = M2F(hr) - M2F(hl);
	chunk->normals[idx][1] = 2.0f;
	chunk->normals[idx][2] = M2F(hd) - M2F(hu);
	glm_vec3_normalize(chunk->normals[idx]);
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

	{
		cgl_t *cgl = &chunk->gl[CDL_X];
		cgl_unbind(cgl);
		int w = 0;
		int i = CHUNK_SIZE - 2;
		int idx;
		cgl->npoints = 4 * (CHUNK_SIZE - 2);
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int j = 1; j < CHUNK_SIZE - 1; j++, w += 2) {
			idx = i * CHUNK_SIZE + j;
			/* warn("idx %d w %d\n", idx, w); */
			glm_vec3_copy(chunk->vertices[idx], points[w]);
			glm_vec3_copy(chunk->normals[idx], normals[w]);
			glm_vec3_copy(chunk->vertices[idx + CHUNK_SIZE], points[w + 1]);
			glm_vec3_copy(chunk->normals[idx + CHUNK_SIZE], normals[w + 1]);
		}

		i++;
		idx = i * CHUNK_SIZE;
		for (int j = 1; j < CHUNK_SIZE - 1; j++, w += 2) {
			glm_vec3_copy(chunk->vertices[idx + j], points[w]);
			glm_vec3_copy(chunk->normals[idx + j], normals[w]);
			cgl_bind_3f(points[w + 1],
				    chunk->vertices[idx][0] + 1,
				    M2F(chunkx->m[j]),
				    chunk->vertices[idx][2] + j);
			glm_vec3_copy(chunkx->normals[j], normals[w + 1]);
		}

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunkx->gl[CDL_NX];
		int w = 0;
		cgl_unbind(cgl);
		cgl->npoints = 2 * (CHUNK_SIZE - 2);
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int j = 1; j < CHUNK_SIZE - 1; j++, w += 2) {
			int idx = j;
			glm_vec3_copy(chunkx->vertices[idx], points[w]);
			glm_vec3_copy(chunkx->normals[idx], normals[w]);
			glm_vec3_copy(chunkx->vertices[idx + CHUNK_SIZE], points[w + 1]);
			glm_vec3_copy(chunkx->normals[idx + CHUNK_SIZE], normals[w + 1]);
		}

		cgl_bind(cgl, points, normals, NULL);
	}
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

	{
		cgl_t *cgl = &chunky->gl[CDL_NY];
		cgl_unbind(cgl);
		int w = 0;
		cgl->npoints = 2 * (CHUNK_SIZE - 2);
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int i = 1; i < CHUNK_SIZE - 1; i++, w += 2) {
			int idx = i * CHUNK_SIZE;
			/* warn("idx %d w %d\n", idx, w); */
			glm_vec3_copy(chunky->vertices[idx], points[w]);
			glm_vec3_copy(chunky->normals[idx], normals[w]);
			glm_vec3_copy(chunky->vertices[idx + 1], points[w + 1]);
			glm_vec3_copy(chunky->normals[idx + 1], normals[w + 1]);
		}

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk->gl[CDL_Y];
		cgl_unbind(cgl);
		int w = 0;
		cgl->npoints = 4 * (CHUNK_SIZE - 2);
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int i = 1; i < CHUNK_SIZE - 1; i++, w += 2) {
			int j = CHUNK_SIZE - 2;
			int idx = i * CHUNK_SIZE + j;
			/* warn("idx %d w %d\n", idx, w); */
			glm_vec3_copy(chunk->vertices[idx], points[w]);
			glm_vec3_copy(chunk->normals[idx], normals[w]);
			glm_vec3_copy(chunk->vertices[idx + 1], points[w + 1]);
			glm_vec3_copy(chunk->normals[idx + 1], normals[w + 1]);
		}

		for (int i = 1; i < CHUNK_SIZE - 1; i++, w += 2) {
			int j = CHUNK_SIZE - 1;
			int idx = i * CHUNK_SIZE + j;
			glm_vec3_copy(chunk->vertices[idx], points[w]);
			glm_vec3_copy(chunk->normals[idx], normals[w]);
			cgl_bind_3f(points[w + 1],
				chunk->vertices[idx][0],
				M2F(chunky->m[idx - j]),
				chunk->vertices[idx][2] + 1);
			glm_vec3_copy(chunky->normals[idx - j], normals[w + 1]);
		}

		cgl_bind(cgl, points, normals, NULL);
	}
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

	{
		cgl_t *cgl = &chunk->gl[CDL_XY];
		cgl_unbind(cgl);
		cgl->npoints = 12;
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];
		int i = CHUNK_SIZE - 1;
		int j = CHUNK_SIZE - 1;
		int idx = i * CHUNK_SIZE + j;

		glm_vec3_copy(chunk->vertices[idx - 1 - CHUNK_SIZE], points[0]);
		glm_vec3_copy(chunk->normals[idx - 1 - CHUNK_SIZE], normals[0]);
		glm_vec3_copy(chunk->vertices[idx - 1], points[1]);
		glm_vec3_copy(chunk->normals[idx - 1], normals[1]);
		glm_vec3_copy(chunk->vertices[idx - CHUNK_SIZE], points[2]);
		glm_vec3_copy(chunk->normals[idx - CHUNK_SIZE], normals[2]);
		glm_vec3_copy(chunk->vertices[idx], points[3]);
		glm_vec3_copy(chunk->normals[idx], normals[3]);
		cgl_bind_3f(points[4],
			chunk->vertices[idx][0] - 1,
			M2F(chunky->m[idx - j - CHUNK_SIZE]),
			chunk->vertices[idx][2] + 1);
		glm_vec3_copy(chunky->normals[idx - j - CHUNK_SIZE], normals[4]);
		cgl_bind_3f(points[5],
			chunk->vertices[idx][0],
			M2F(chunky->m[idx - j]),
			chunk->vertices[idx][2] + 1);
		glm_vec3_copy(chunky->normals[idx - j], normals[5]);

		glm_vec3_copy(chunk->vertices[idx - 1], points[6]);
		glm_vec3_copy(chunk->normals[idx - 1], normals[6]);
		cgl_bind_3f(points[7],
			    chunk->vertices[idx][0] + 1,
			    M2F(chunkx->m[j - 1]),
			    chunk->vertices[idx][2] - 1);
		glm_vec3_copy(chunkx->normals[j - 1], normals[7]);
		glm_vec3_copy(chunk->vertices[idx], points[8]);
		glm_vec3_copy(chunk->normals[idx], normals[8]);
		cgl_bind_3f(points[9],
			    chunk->vertices[idx][0] + 1,
			    M2F(chunkx->m[j]),
			    chunk->vertices[idx][2]);
		glm_vec3_copy(chunkx->normals[j], normals[9]);
		cgl_bind_3f(points[10],
			    chunk->vertices[idx][0],
			    M2F(chunky->m[idx - j]),
			    chunk->vertices[idx][2] + 1);
		glm_vec3_copy(chunky->normals[idx - j], normals[10]);
		cgl_bind_3f(points[11],
			    chunk->vertices[idx][0] + 1,
			    M2F(chunkxy->m[0]),
			    chunk->vertices[idx][2] + 1);
		glm_vec3_copy(chunkxy->normals[0], normals[11]);

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunkxy->gl[CDL_NXNY];
		cgl_unbind(cgl);
		cgl->npoints = 4;
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		glm_vec3_copy(chunkxy->vertices[0], points[0]);
		glm_vec3_copy(chunkxy->normals[0], normals[0]);
		glm_vec3_copy(chunkxy->vertices[CHUNK_SIZE], points[1]);
		glm_vec3_copy(chunkxy->normals[CHUNK_SIZE], normals[1]);
		glm_vec3_copy(chunkxy->vertices[1], points[2]);
		glm_vec3_copy(chunkxy->normals[1], normals[2]);
		glm_vec3_copy(chunkxy->vertices[CHUNK_SIZE + 1], points[3]);
		glm_vec3_copy(chunkxy->normals[CHUNK_SIZE + 1], normals[3]);

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunky->gl[CDL_XNY];
		cgl_unbind(cgl);
		cgl->npoints = 6;
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];
		unsigned i = CHUNK_SIZE - 2;
		unsigned j = 0;
		unsigned idx = i * CHUNK_SIZE + j;

		glm_vec3_copy(chunky->vertices[idx], points[0]);
		glm_vec3_copy(chunky->normals[idx], normals[0]);
		glm_vec3_copy(chunky->vertices[idx + 1], points[1]);
		glm_vec3_copy(chunky->normals[idx + 1], normals[1]);
		glm_vec3_copy(chunky->vertices[idx + CHUNK_SIZE], points[2]);
		glm_vec3_copy(chunky->normals[idx + CHUNK_SIZE], normals[2]);
		glm_vec3_copy(chunky->vertices[idx + CHUNK_SIZE + 1], points[3]);
		glm_vec3_copy(chunky->normals[idx + CHUNK_SIZE + 1], normals[3]);
		cgl_bind_3f(points[4],
			chunky->vertices[idx][0] + 2,
			M2F(chunkxy->m[0]),
			chunky->vertices[idx][2]);
		glm_vec3_copy(chunkxy->normals[0], normals[4]);
		cgl_bind_3f(points[5],
			chunky->vertices[idx][0] + 2,
			M2F(chunkxy->m[1]),
			chunky->vertices[idx][2] + 1);
		glm_vec3_copy(chunkxy->normals[1], normals[5]);

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunkx->gl[CDL_NXY];
		cgl_unbind(cgl);
		cgl->npoints = 8;
		unsigned idx = CHUNK_SIZE - 2;
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		cgl_bind_3f(points[0],
			chunkx->vertices[idx][0],
			M2F(chunkx->m[idx]),
			chunkx->vertices[idx][2]);
		glm_vec3_copy(chunkx->normals[idx], normals[0]);

		cgl_bind_3f(points[1],
			chunkx->vertices[idx][0] + 1,
			M2F(chunkx->m[idx + CHUNK_SIZE]),
			chunkx->vertices[idx][2]);
		glm_vec3_copy(chunkx->normals[idx + CHUNK_SIZE], normals[1]);

		idx ++;

		cgl_bind_3f(points[2],
			chunkx->vertices[idx][0],
			M2F(chunkx->m[idx]),
			chunkx->vertices[idx][2]);
		glm_vec3_copy(chunkx->normals[idx], normals[2]);

		cgl_bind_3f(points[3],
			chunkx->vertices[idx][0] + 1,
			M2F(chunkx->m[idx + CHUNK_SIZE]),
			chunkx->vertices[idx][2]);
		glm_vec3_copy(chunkx->normals[idx + CHUNK_SIZE], normals[3]);

		/* idx++; */

		cgl_bind_3f(points[4],
			chunkx->vertices[idx][0],
			M2F(chunkxy->m[0]),
			chunkx->vertices[idx][2] + 1);
		glm_vec3_copy(chunkxy->normals[0], normals[4]);

		cgl_bind_3f(points[5],
			chunkx->vertices[idx][0] + 1,
			M2F(chunkxy->m[CHUNK_SIZE]),
			chunkx->vertices[idx][2] + 1);
		glm_vec3_copy(chunkxy->normals[CHUNK_SIZE], normals[5]);

		cgl_bind(cgl, points, normals, NULL);
	}
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
	memset(chunk.gl, 0, sizeof(chunk.gl));

	for (int i = 0; i < CHUNK_SIZE; i++)
		for (int j = 0; j < CHUNK_SIZE; j++) {
			int idx = i * CHUNK_SIZE + j;
			chunk.vertices[idx][0] = (double) i;
			chunk.vertices[idx][1] = M2F(chunk.m[idx]);
			chunk.vertices[idx][2] = (double) j;
			noise_t hh = chunk.m[idx];
			normal_fix(&chunk, idx,
				   idx - CHUNK_SIZE >= 0 ? chunk.m[idx - CHUNK_SIZE] : hh,
				   idx - 1 >= 0 ? chunk.m[idx - 1] : hh,
				   idx + CHUNK_SIZE < CHUNK_M ? chunk.m[idx + CHUNK_SIZE] : hh,
				   idx + 1 < CHUNK_M ? chunk.m[idx + 1] : hh);
		}

	{
		cgl_t *cgl = &chunk.gl[CDL_CENTER];
		cgl_prebind(cgl, 0);
		int w = 0;
		cgl->npoints = 2 * ((CHUNK_SIZE - 3) * (CHUNK_SIZE - 2) + (CHUNK_SIZE - 2));
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int i = 1; i < CHUNK_SIZE - 2; i++) {
			for (int j = 1; j < CHUNK_SIZE - 1; j++, w += 2) {
				int idx = i * CHUNK_SIZE + j;
				/* warn("idx %d w %d\n", idx, w); */
				glm_vec3_copy(chunk.vertices[idx], points[w]);
				glm_vec3_copy(chunk.normals[idx], normals[w]);
				glm_vec3_copy(chunk.vertices[idx + CHUNK_SIZE], points[w + 1]);
				glm_vec3_copy(chunk.normals[idx + CHUNK_SIZE], normals[w + 1]);
			}
		}
		w -= 2;

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk.gl[CDL_NX];
		cgl_prebind(cgl, 0);
		int w = 0;
		cgl->npoints = 2 * (CHUNK_SIZE - 2);
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int j = 1; j < CHUNK_SIZE - 1; j++, w += 2) {
			int idx = j;
			/* warn("idx %d w %d\n", idx, w); */
			glm_vec3_copy(chunk.vertices[idx], points[w]);
			glm_vec3_copy(chunk.normals[idx], normals[w]);
			glm_vec3_copy(chunk.vertices[idx + CHUNK_SIZE], points[w + 1]);
			glm_vec3_copy(chunk.normals[idx + CHUNK_SIZE], normals[w + 1]);
		}

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk.gl[CDL_X];
		cgl_prebind(cgl, 0);
		int w = 0;
		int i = CHUNK_SIZE - 2;
		cgl->npoints = 2 * (CHUNK_SIZE - 2);
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int j = 1; j < CHUNK_SIZE - 1; j++, w += 2) {
			int idx = i * CHUNK_SIZE + j;
			/* warn("idx %d w %d\n", idx, w); */
			glm_vec3_copy(chunk.vertices[idx], points[w]);
			glm_vec3_copy(chunk.normals[idx], normals[w]);
			glm_vec3_copy(chunk.vertices[idx + CHUNK_SIZE], points[w + 1]);
			glm_vec3_copy(chunk.normals[idx + CHUNK_SIZE], normals[w + 1]);
		}

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk.gl[CDL_NY];
		cgl_prebind(cgl, 0);
		int w = 0;
		cgl->npoints = 2 * (CHUNK_SIZE - 2);
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int i = 1; i < CHUNK_SIZE - 1; i++, w += 2) {
			int idx = i * CHUNK_SIZE;
			/* warn("idx %d w %d\n", idx, w); */
			glm_vec3_copy(chunk.vertices[idx], points[w]);
			glm_vec3_copy(chunk.normals[idx], normals[w]);
			glm_vec3_copy(chunk.vertices[idx + 1], points[w + 1]);
			glm_vec3_copy(chunk.normals[idx + 1], normals[w + 1]);
		}

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk.gl[CDL_Y];
		cgl_prebind(cgl, 0);
		int w = 0;
		cgl->npoints = 2 * (CHUNK_SIZE - 2);
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		for (int i = 1; i < CHUNK_SIZE - 1; i++, w += 2) {
			int j = CHUNK_SIZE - 2;
			int idx = i * CHUNK_SIZE + j;
			/* warn("idx %d w %d\n", idx, w); */
			glm_vec3_copy(chunk.vertices[idx], points[w]);
			glm_vec3_copy(chunk.normals[idx], normals[w]);
			glm_vec3_copy(chunk.vertices[idx + 1], points[w + 1]);
			glm_vec3_copy(chunk.normals[idx + 1], normals[w + 1]);
		}

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk.gl[CDL_NXY];
		cgl_prebind(cgl, 0);
		cgl->npoints = 0;
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];
		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk.gl[CDL_XY];
		cgl_prebind(cgl, 0);
		cgl->npoints = 0;
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];
		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk.gl[CDL_NXNY];
		cgl_prebind(cgl, 0);
		cgl->npoints = 4;
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];

		glm_vec3_copy(chunk.vertices[0], points[0]);
		glm_vec3_copy(chunk.normals[0], normals[0]);
		glm_vec3_copy(chunk.vertices[CHUNK_SIZE], points[1]);
		glm_vec3_copy(chunk.normals[CHUNK_SIZE], normals[1]);
		glm_vec3_copy(chunk.vertices[1], points[2]);
		glm_vec3_copy(chunk.normals[1], normals[2]);
		glm_vec3_copy(chunk.vertices[CHUNK_SIZE + 1], points[3]);
		glm_vec3_copy(chunk.normals[CHUNK_SIZE + 1], normals[3]);

		cgl_bind(cgl, points, normals, NULL);
	}

	{
		cgl_t *cgl = &chunk.gl[CDL_XNY];
		cgl_prebind(cgl, 0);
		cgl->npoints = 4;
		vec3 points[cgl->npoints];
		vec3 normals[cgl->npoints];
		unsigned i = CHUNK_SIZE - 2;
		unsigned j = 0;
		unsigned idx = i * CHUNK_SIZE + j;

		glm_vec3_copy(chunk.vertices[idx], points[0]);
		glm_vec3_copy(chunk.normals[idx], normals[0]);
		glm_vec3_copy(chunk.vertices[idx + 1], points[1]);
		glm_vec3_copy(chunk.normals[idx + 1], normals[1]);
		glm_vec3_copy(chunk.vertices[idx + CHUNK_SIZE], points[2]);
		glm_vec3_copy(chunk.normals[idx + CHUNK_SIZE], normals[2]);
		glm_vec3_copy(chunk.vertices[idx + CHUNK_SIZE + 1], points[3]);
		glm_vec3_copy(chunk.normals[idx + CHUNK_SIZE + 1], normals[3]);

		cgl_bind(cgl, points, normals, NULL);
	}

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
	glm_mat4_identity(modelm);
	vec3 translation = { chunk->pos[0], 0, chunk->pos[1] };
	glm_translate(modelm, translation);
	glUniformMatrix4fv(modelLoc, 1, 0, &modelm[0][0]);

	glBindTexture(GL_TEXTURE_2D, white_texture);

	glBindVertexArray(chunk->gl[CDL_CENTER].vao);
	for (int i = 0; i < (CHUNK_SIZE - 2) * 2 - 2; i += 2)
		glDrawArrays(GL_TRIANGLE_STRIP, (CHUNK_SIZE - 2) * i, (CHUNK_SIZE - 2) * 2);

	glBindVertexArray(chunk->gl[CDL_NXNY].vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindVertexArray(chunk->gl[CDL_XNY].vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, chunk->gl[CDL_XNY].npoints);

	glBindVertexArray(chunk->gl[CDL_NX].vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, (CHUNK_SIZE - 2) * 2);

	glBindVertexArray(chunk->gl[CDL_X].vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, (CHUNK_SIZE - 2) * 2);
	if (chunk->gl[CDL_X].updated)
		glDrawArrays(GL_TRIANGLE_STRIP, (CHUNK_SIZE - 2) * 2, (CHUNK_SIZE - 2) * 2);

	glBindVertexArray(chunk->gl[CDL_NY].vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, (CHUNK_SIZE - 2) * 2);

	if (chunk->gl[CDL_NXY].updated) {
		glBindVertexArray(chunk->gl[CDL_NXY].vao);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
	}

	glBindVertexArray(chunk->gl[CDL_Y].vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, (CHUNK_SIZE - 2) * 2);
	if (chunk->gl[CDL_Y].updated)
		glDrawArrays(GL_TRIANGLE_STRIP, (CHUNK_SIZE - 2) * 2, (CHUNK_SIZE - 2) * 2);

	if (chunk->gl[CDL_XY].updated) {
		glBindVertexArray(chunk->gl[CDL_XY].vao);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
		glDrawArrays(GL_TRIANGLE_STRIP, 6, 6);
	}

	glPopMatrix();

	GL_DEBUG("chunk_render");
}

void chunks_render() {
	int16_t min[4] = { - CHUNK_SIZE * 2, - CHUNK_SIZE * 2, 0, 0 };
	int16_t max[4] = { CHUNK_SIZE * 2 - 1, CHUNK_SIZE * 2 - 1, 0, 0 };
	chunks_sdb.callback = &chunk_render;
	sdb_search(&chunks_sdb, min, max);
}
