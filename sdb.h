#ifndef SDB_H
#define SDB_H

// see http://www.vision-tools.com/h-tropf/multidimensionalrangequery.pdf

#include <db4/db.h>
/* value returned when no position is found */
#define SDB_INVALID 130056652770671ULL

typedef void (*sdb_callback_t)(int16_t *pv, void *ptr);

typedef struct {
	DB *point2ptr;
	DB *ptr2point;
	unsigned char dim;
	size_t el_len;
	sdb_callback_t callback;
} sdb_t;

int sdb_init(sdb_t *, unsigned char dim, size_t el_len, DB_ENV *dbe, char *fname);
void sdb_search(sdb_t *, int16_t *min, int16_t *max);
int sdb_close(sdb_t *);
void sdb_where(int16_t *p, sdb_t *, void *thing);
int sdb_delete(sdb_t *, void *what);
void *sdb_get(sdb_t *, int16_t *at);
void sdb_put(sdb_t *, int16_t *p, void *thing, int flags);

#ifdef SDB_IMPLEMENTATION
#include "debug.h"

typedef uint64_t morton_t; // up to 4d morton
typedef int64_t smorton_t;

static const morton_t m1 = 0x111111111111ULL;
static const morton_t m0 = 0x800000000000ULL;

morton_t
pos_morton(int16_t *p, uint8_t dim)
{
	morton_t res = 0;
	int i, j;

	for (i = 0; i < dim; i++)
		for (j = 0; j < 16; j++) {
			register uint16_t v = (smorton_t) p[i] + SHRT_MAX;
			if (v == USHRT_MAX)
				v = 1;
			res |= ((morton_t) (v >> j) & 1) << (j * dim + i);
		}

	/* res <<= (4 - dim) * 16; */

	return res;
}

/* leave me alone */
void
morton_pos(int16_t *p, morton_t code, uint8_t dim)
{
	uint16_t up[dim];
	int i, j;

	for (i = 0; i < dim; i++) {
		up[i] = 0;
		for (j = 0; j < 16; j++) {
			int16_t v = (code >> (j * dim + i)) & 1;
			/* int16_t v = (code >> (j * dim + i + (4 - dim) * 16)) & 1; */
			up[i] |= v << j;
		}
	}

	for (i = 0; i < dim; i++) {
		p[i] = (smorton_t) up[i] - SHRT_MAX;
	}
}

static inline void
point_debug(int16_t *p, char *label, int8_t dim)
{
	warn("point %s", label);
	for (int i = 0; i < dim; i++)
		warn(" %d", p[i]);
	warn("\n");
}

static inline void
morton_debug(morton_t x, uint8_t dim)
{
	int16_t p[dim];
	morton_pos(p, x, dim);
	warn("%llx ", x);
	point_debug(p, "morton_debug", dim);
}

static inline int
morton_cmp(morton_t a, morton_t b, uint8_t dim, uint8_t cdim) {
	/* unsigned offset = (4 - dim) * 16; */
	/* int j; */

	/* for (j = 0; j < 16; j++) { */
	/* 	unsigned idx = 1 << (4 * 16 - offset - (j + dim * cdim)); */
	/* 	if (b & idx) { */
	/* 		if (a & idx) */
	/* 			continue; */
	/* 		else */
	/* 			return 1; */
	/* 	} else if (a & idx) */
	/* 		return -1; */
	/* } */

	/* return 0; */
	int16_t av[dim], bv[dim];
	morton_pos(av, a, dim);
	morton_pos(bv, b, dim);
	if (av[cdim] > bv[cdim])
		return -1;
	else if (av[cdim] < bv[cdim])
		return 1;
	else
		return 0;
}

static inline morton_t
sdb_qload_0(morton_t c, morton_t lm0, morton_t lm1) // LOAD(0111...
{
	return (c & (~ lm0)) | lm1;
}

static inline morton_t
sdb_qload_1(morton_t c, morton_t lm0, morton_t lm1) // LOAD(1000...
{
	return (c | lm0) & (~ lm1);
}

static inline int
sdb_inrange(morton_t dr, morton_t min, morton_t max, uint8_t dim)
{
	int16_t drv[dim], minv[dim], maxv[dim];

	morton_pos(drv, dr, dim);
	morton_pos(minv, min, dim);
	morton_pos(maxv, max, dim);

	/* warn("sdb_inrange %llx %llx %llx\n", min, dr, max); */
	/* morton_debug(min, dim); */
	/* morton_debug(dr, dim); */
	/* morton_debug(max, dim); */

	for (int i = 0; i < dim; i++) {
		if (drv[i] < minv[i] || drv[i] > maxv[i])
			return 0;
	}

	/* for (int i = 0; i < dim; i++) */
	/* 	if (morton_cmp(min, dr, dim, i) < 0 */
	/* 	    || morton_cmp(dr, max, dim, i) >= 0) */
	/* 	    return 0; */

	return 1;
}

static inline void
sdb_compute_bmlm(morton_t *bm, morton_t *lm,
	     morton_t dr, morton_t min, morton_t max)
{
	register morton_t lm0 = m0, lm1 = m1;

	for (; lm0; lm0 >>= 1, lm1 >>= 1)
	{
		register morton_t a = dr & lm0,
			 b = min & lm0,
			 c = max & lm0;

		if (b) { // ? 1 ?
			if (!a && c) { // 0 1 1
				*bm = min;
				break;
			}

		} else if (a) // 1 0 ?
			if (c) { // 1 0 1
				*lm = sdb_qload_0(max, lm0, lm1);
				min = sdb_qload_1(min, lm0, lm1);
			} else { // 1 0 0
				*lm = max;
				break;
			}

		else if (c) { // 0 0 1
			// BIGMIN
			*bm = sdb_qload_1(min, lm0, lm1);
			max = sdb_qload_0(max, lm0, lm1);
		}
	}
}

static inline int
sdb_range_unsafe(sdb_t *sdb, morton_t min, morton_t max)
{
	morton_t lm = 0, code;
	DBC *cur;
	DBT key, pkey, data;
	int ret = 0, res = 0;
	int dbflags;
	int16_t minv[sdb->dim], maxv[sdb->dim];
	morton_pos(minv, min, sdb->dim);
	morton_pos(maxv, max, sdb->dim);

	warn("sdb_range_unsafe min %llx max %llx\n", min, max);
	morton_debug(min, sdb->dim);
	morton_debug(max, sdb->dim);
	/* CBUG((smorton_t) min > (smorton_t) max); */
	if (sdb->point2ptr->cursor(sdb->point2ptr, NULL, &cur, 0))
		return -1;

	memset(&data, 0, sizeof(DBT));
	memset(&pkey, 0, sizeof(DBT));

	dbflags = DB_SET_RANGE;
	memset(&key, 0, sizeof(DBT));
	key.data = &min;
	key.size = sizeof(min);

next:	ret = cur->c_pget(cur, &key, &pkey, &data, dbflags);
	dbflags = DB_NEXT;

	switch (ret) {
	case 0: break;
	case DB_NOTFOUND:
		ret = 0;
		goto out;
	default:
		BUG("%s", db_strerror(ret));
		goto out;
	}

	code = *(morton_t*) key.data;

	if ((smorton_t) code > (smorton_t) max)
		goto out;

	/* warn("found chunk at %llx\n", code); */
	if (sdb_inrange(code, min, max, sdb->dim)) {
		warn("%llx in range!\n", code);
		int16_t pv[sdb->dim];
		morton_pos(pv, code, sdb->dim);
		sdb->callback(pv, pkey.data);
		res++;
	} else
		sdb_compute_bmlm(&min, &lm, code, min, max);

	goto next;

out:	if (cur->close(cur) || ret)
		return -1;

	return res;
}

static inline int16_t
morton_get(morton_t x, uint8_t i, uint8_t dim)
{
	int16_t p[dim];
	morton_pos(p, x, dim);
	return p[i];
}

static inline morton_t
morton_set(morton_t x, uint8_t i, int16_t value, uint8_t dim)
{
	int16_t p[dim];
	morton_pos(p, x, dim);
	p[i] = value;
	return pos_morton(p, dim);
}

/* this version accounts for type limits,
 * and wraps around them in each direction, if necessary
 * TODO iterative form
 */

static int
sdb_range_safe(sdb_t *sdb, morton_t min, morton_t max, int dim)
{
	int i, ret, aux;
	morton_t lmin = min,
		 lmax = max;

	/* warn("sdb_range_safe %d %llx %llx\n", dim, min, max); */
	/* morton_debug(min, sdb->dim); */
	/* morton_debug(max, sdb->dim); */

	for (i = dim; i < sdb->dim; i++) {
		// FIXME account for world limits (might be different from data type limits)

		int16_t lmaxi = morton_get(lmax, i, sdb->dim);
		int16_t lmini = morton_get(lmin, i, sdb->dim);

		if (lmaxi >= lmini) // didn't cross data type limit
			continue;

		// lmin is correct
		lmax = morton_set(lmax, i, SHRT_MAX, sdb->dim);
		aux = sdb_range_safe(sdb, lmin, lmax, i + 1);

		if (aux < 0)
			return aux;

		lmin = morton_set(lmin, i, SHRT_MIN, sdb->dim);
		lmax = morton_set(lmax, i, lmaxi - 1, sdb->dim);
		ret = sdb_range_safe(sdb, lmin, lmax, i + 1);

		if (ret < 0)
			return ret;

		return aux + ret;
	}

	return sdb_range_unsafe(sdb, lmin, lmax);
}

static inline morton_t
morton_unscramble(morton_t code, uint8_t dim)
{
	morton_t result;
	morton_pos((int16_t *) &result, code, dim);
	return result;
}

// FIXME there is a memory leak in which sdb gets overwritten
void
sdb_search(sdb_t *sdb, int16_t *min, int16_t *max)
{
	morton_t morton_min = pos_morton(min, sdb->dim),
		 morton_max = pos_morton(max, sdb->dim);
	int ret;

	warn("sdb_search %llx %llx\n", morton_min, morton_max);

	ret = sdb_range_safe(sdb, morton_min, morton_max, 0);
	warn("sdb_search result %d\n", ret);
}

static int
sdb_cmp(DB *sec, const DBT *a_r, const DBT *b_r)
{
	morton_t a = * (morton_t*) a_r->data,
		 b = * (morton_t*) b_r->data;

	return b > a ? -1 : (a > b ? 1 : 0);
}

static int
sdb_mki_code(DB *sec, const DBT *key, const DBT *data, DBT *result)
{
	result->size = sizeof(morton_t);
	result->data = data->data;
	return 0;
}

int
sdb_init(sdb_t *sdb, unsigned char dim, size_t el_len, DB_ENV *dbe, char *fname) {
	int ret = 0;

	sdb->dim = dim;
	sdb->el_len = el_len;

	if ((ret = db_create(&sdb->ptr2point, dbe, 0))
	    || (ret = sdb->ptr2point->open(sdb->ptr2point, NULL, fname, fname, DB_HASH, DB_CREATE, 0664)))
		return ret;

	if ((ret = db_create(&sdb->point2ptr, dbe, 0))
	    || (ret = sdb->point2ptr->set_bt_compare(sdb->point2ptr, sdb_cmp))
	    || (ret = sdb->point2ptr->open(sdb->point2ptr, NULL, fname, fname, DB_BTREE, DB_CREATE, 0664))
	    || (ret = sdb->ptr2point->associate(sdb->ptr2point, NULL, sdb->point2ptr, sdb_mki_code, DB_CREATE)))
	
		sdb->point2ptr->close(sdb->point2ptr, 0);
	else
		return 0;

	sdb->ptr2point->close(sdb->ptr2point, 0);
	return ret;
}

int
sdb_close(sdb_t *sdb)
{
	return sdb->point2ptr->close(sdb->point2ptr, 0)
		|| sdb->ptr2point->close(sdb->ptr2point, 0);
}

static void
_sdb_put(sdb_t *sdb, morton_t code, void *thing, int flags)
{
	DBT key;
	DBT data;
	int ret;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.data = thing;
	key.size = sdb->el_len;
	data.data = &code;
	data.size = sizeof(code);

	ret = sdb->ptr2point->put(sdb->ptr2point, NULL, &key, &data, flags);
	warn("put a chunk at %llx\n", code);
	CBUG(ret);
}

void
sdb_put(sdb_t *sdb, int16_t *p, void *thing, int flags)
{
	morton_t code = pos_morton(p, sdb->dim);
	_sdb_put(sdb, code, thing, flags);
	point_debug(p, "sdb_put", sdb->dim);
}

morton_t
_sdb_where(sdb_t *sdb, void *thing)
{
	int bad;
	DBT key;
	DBT data;

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	key.data = thing;
	key.size = sdb->el_len;

	if ((bad = sdb->ptr2point->get(sdb->ptr2point, NULL, &key, &data, 0))) {
		static morton_t code = SDB_INVALID;
		if (bad && bad != DB_NOTFOUND)
			warn("sdb_where %p %s", thing, db_strerror(bad));
		return code;
	}

	return * (morton_t *) data.data;
}

void
sdb_where(int16_t *p, sdb_t *sdb, void *thing)
{
	morton_t x = _sdb_where(sdb, thing);
	morton_pos(p, x, sdb->dim);
}

void *
sdb_get(sdb_t *sdb, int16_t *p)
{
	morton_t at = pos_morton(p, sdb->dim);
	DBC *cur;
	DBT pkey;
	DBT data;
	DBT key;
	int res;

	if (sdb->point2ptr->cursor(sdb->point2ptr, NULL, &cur, 0))
		return NULL;

	memset(&pkey, 0, sizeof(DBT));
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.size = sizeof(at);
	key.data = &at;

	while (1) {
		res = cur->c_pget(cur, &key, &pkey, &data, DB_SET);
		switch (res) {
		case 0:
			return pkey.data;
		case DB_NOTFOUND:
			cur->close(cur);
			return NULL;
		default:
			BUG("%s", db_strerror(res));
		}
	}
}

int
sdb_delete(sdb_t *sdb, void *what)
{
	DBT key;
	morton_t code = _sdb_where(sdb, what);
	memset(&key, 0, sizeof(key));
	key.data = &code;
	key.size = sizeof(code);
	return sdb->point2ptr->del(sdb->point2ptr, NULL, &key, 0);
}

#endif

#endif
