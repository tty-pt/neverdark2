/* A noise algorithm that wraps well around data type limits.
 *
 * LICENCE: CC BY-NC-SA 4.0
 * For commercial use, contact me at q@qnixsoft.com
 *
 * DEFINITIONS:
 *
 * a point has DIM coordinates
 *
 * a matrix has 2^y (l) edge length.
 * It is an array of noise_t where we want to store the result.
 *
 * A noise feature quad has 2^x (d) edge length
 * and 2^DIM vertices with random values.
 *
 * Other values are the result of a "fade" between those.
 * And they are added to the resulting matrix (for multiple octaves);
 *
 * v is an array that stores a (noise feature) quad's vertices.
 * It has a peculiar order FIXME
 *
 * recursive implementations are here for reference
 * TODO further optimizations
 * TODO does this work in 3D+?
 * TODO improve documentation
 * TODO speed tests
 * TODO think before changing stuff
 * */

#ifndef NOISE_H
#define NOISE_H

#include <string.h>
#include "hash.h"
#include "xxhash.h"
#include "debug.h"

#define NOISE_MAX ((noise_t) -1)

typedef uint32_t noise_t;
typedef int32_t snoise_t;

typedef struct { unsigned x, w; } octave_t;

#define HASH XXH32

/* gets a mask for use with get_s */
#define COORDMASK_LSHIFT(x) (uint16_t) ((((uint16_t) -1) << x))

/* gets a (deterministic) random value for point p */
#define NOISE_R(dim) static inline noise_t \
	noise_r ## dim (int16_t p[dim], unsigned seed, unsigned w) { \
		register noise_t v = HASH(p, sizeof(int16_t) * dim, seed); \
		return ((long long unsigned) v) >> w; \
	}

/* generate values at quad vertices. */
#define NOISE_GET_V(dim) static inline void \
	noise_get_v ## dim (noise_t v[1 << dim], int16_t s[dim], uint16_t x, unsigned w, unsigned seed) { \
		const int16_t d = 1 << x; \
		/* TODO: make this dynamic to dim */ \
		const int16_t va[][dim] = { \
			{ s[0], s[1] }, \
			{ s[0], s[1] + d }, \
			{ s[0] + d, s[1] }, \
			{ s[0] + d, s[1] + d } \
		}; \
		int i; \
		\
		for (i = 0; i < (1 << dim); i++) \
		v[i] = noise_r ## dim ((int16_t *) va[i], seed, w); \
	}


static inline void
calc_steps(snoise_t *st,
	   noise_t *v,
	   uint16_t z,
	   uint16_t vl,
	   unsigned y)
{
	int n;
	for (n = 0; n < vl; n ++)
		// FIXME FIND PARENS
		st[n] = ((long) v[n + vl] - v[n]) >> z << y;
}

static inline void
step(noise_t *v, snoise_t *st, uint16_t vl, uint16_t mul)
{
	int n;
	for (n = 0; n < vl; n++)
		v[n] = (long) v[n] + ((long) st[n] * mul);
}

/* Given a set of vertices,
 * fade between them and set target values acoordingly
 */
#define NOISE_QUAD(dim) static inline void \
	noise_quad ## dim (noise_t *c, noise_t *vc, unsigned z, unsigned w, unsigned cy) { \
		uint16_t ndim = dim - 1; \
		uint16_t tvl = 1 << ndim; /* length of input values */ \
		snoise_t st[(tvl<<1) - 1], *stc = st; \
		noise_t *ce_p[dim], *vt; \
		size_t cd = 1 << cy * ndim; /* (2^y)^ndim */ \
		goto start; \
		do { \
			do { \
				/* PUSH */ \
				cd >>= cy; ndim--; vc = vt; stc += tvl; tvl >>= 1; \
				\
				start:			ce_p[ndim] = c + (cd<<z); \
				vt = vc + (tvl<<1); \
				\
				calc_steps(stc, vc, z, tvl, 0); \
				memcpy(vt, vc, tvl * sizeof(noise_t)); \
			} while (ndim); \
			\
			for (; c < ce_p[0]; c+=cd, vt[0]+=*stc) \
			*c += vt[0]; \
			\
			do { \
				/* POP */ \
				++ndim; \
				if (ndim >= dim) \
				return; \
				c -= cd << z; cd <<= cy; vt = vc; \
				tvl <<= 1; stc -= tvl; vc -= (tvl<<1); \
				\
				step(vt, stc, tvl, 1); \
				c += cd; \
			} while (c >= ce_p[ndim]); \
		} while (1); \
	}

#define _NOISE_M(dim) static inline void \
	_noise_m ## dim(noise_t *c, noise_t *v, unsigned x, int16_t qs[dim], unsigned w, unsigned seed, unsigned cy) { \
		noise_t *ce_p[dim]; \
		noise_t ced = 1<<(cy * dim); \
		int16_t *qsc = qs; /* quad coordinate */ \
		uint16_t ndim = dim - 1; \
		\
		goto start; \
		\
		do { \
			do { /* PUSH */ \
				qsc++; ndim--; \
				start:			ce_p[ndim] = c + ced; \
				ced >>= cy; \
			} while (ndim); \
			\
			do { \
				noise_get_v ## dim (v, qs, x, w, seed); \
				noise_quad ## dim (c, v, x, w, cy); \
				*qsc += 1<<x; \
				c += ced<<x; \
			} while (c < ce_p[ndim]); \
			\
			do { /* POP */ \
				*qsc -= 1 << cy; \
				ced <<= cy; \
				c += (ced<<x) - ced; /* reset and inc */ \
				qsc--; ndim++; \
				*qsc += 1<<x; \
			} while (c >= ce_p[ndim]); \
		} while (ndim < dim); \
	}

/* fixes v (vertex values)
 * when noise quad starts before matrix quad aka x > y aka d > l.
 * assumes ndim to be at least 1
 * vl = 1 << ndim
 */
static inline void
__fix_v(noise_t *v, snoise_t *st, int16_t *ms, int16_t *qs, unsigned x, uint16_t vl, unsigned cy) {
	register noise_t *vn = v + vl;
	register uint16_t dd = (*ms - *qs) >> cy;

	// TODO make this more efficient
	calc_steps(st, v, x, vl, cy);

	if (dd) /* ms > qs */
		step(v, st, vl, dd);

	memcpy(vn, v, sizeof(noise_t) * vl); // COPY to opposite values
	step(vn, st, vl, 1); // STEP by side length of M
}

// yes this does work fine a bit of a hack but ok i guess
#define NOISE_FIX_V(dim) static inline void \
	noise_fix_v ## dim (noise_t *v, int16_t *qs, int16_t *ms, unsigned x, unsigned cy) { \
		uint16_t vl = 1 << dim; \
		snoise_t st[vl - 1], *stc = st; \
		int ndim = dim - 1; \
		int first = 1; \
		vl >>= 1; \
		\
		for (;;) { \
			__fix_v(v, stc, ms, qs, x, vl, cy); \
			\
			if (ndim) { \
				ndim--; qs++; ms++; vl>>=1; stc+=vl; /* PUSH */ \
			} else if (first) { \
				v += vl<<1; \
				first = 0; \
			} else break; \
		} \
	}

#define NOISE_GET_S(dim) static inline void \
	noise_get_s ## dim (int16_t s[dim], int16_t p[dim], uint16_t mask) { \
		int i; \
		for (i = 0; i < dim; i++) \
		s[i] = ((int16_t) p[i]) & mask; \
	}

/* Fills in matrix "mat" with deterministic noise.
 * "ms" is the matrix's min point in the world;
 * "x" 2^x = side length of noise quads;
 * "y" 2^y = side length of matrix "mat".
 * */
#define NOISE_M(dim) static inline void \
	noise_m ## dim (noise_t *mat, int16_t ms[dim], unsigned x, \
			unsigned w, unsigned seed, unsigned cy) { \
		noise_t v[(1 << (dim + 1)) - 1]; \
		\
		if (cy > x) \
			_noise_m ## dim (mat, v, x, ms, w, seed, cy); \
		else { \
			int16_t qs[dim]; \
			\
			noise_get_s ## dim (qs, ms, COORDMASK_LSHIFT(x)); \
			noise_get_v ## dim (v, qs, x, w, seed); \
			if (x > cy) \
				noise_fix_v ## dim (v, qs, ms, x, cy); \
			 \
			noise_quad ## dim (mat, v, cy, w, cy); \
		} \
	}

#define NOISE_OCT(dim) void \
	noise_oct ## dim (noise_t *m, int16_t s[dim], size_t oct_n, \
			  octave_t *oct, unsigned seed, unsigned cm, unsigned cy) { \
		octave_t *oe; \
		memset(m, 0, sizeof(noise_t) * cm); \
		for (oe = oct + oct_n; oct < oe; oct++) \
			noise_m ## dim (m, s, oct->x, oct->w, seed, cy); \
	}

#define NOISE(dim) \
	NOISE_R(dim) \
	NOISE_GET_V(dim) \
	NOISE_QUAD(dim) \
	_NOISE_M(dim) \
	NOISE_FIX_V(dim) \
	NOISE_GET_S(dim) \
	NOISE_M(dim) \
	NOISE_OCT(dim)

#endif
