#ifndef SDB_H
#define SDB_H

// see http://www.vision-tools.com/h-tropf/multidimensionalrangequery.pdf

#include <db4/db.h>
/* value returned when no position is found */
#define SDB_INVALID 130056652770671ULL

typedef struct {
	DB *point2ptr;
	DB *ptr2point;
	unsigned char dim;
	size_t el_len;
} sdb_t;

int sdb_init(sdb_t *, unsigned char dim, size_t el_len, DB_ENV *dbe, char *fname);
void sdb_search(void **mat, sdb_t *, int16_t *min, int16_t *max);
int sdb_close(sdb_t *);
void sdb_where(int16_t *p, sdb_t *, void *thing);
int sdb_delete(sdb_t *, void *what);
void *sdb_get(sdb_t *, int16_t *at);
void sdb_put(sdb_t *, int16_t *p, void *thing, int flags);

#ifdef SDB_IMPLEMENTATION
#include "debug.h"

typedef uint64_t morton_t; // up to 4d morton
typedef int64_t smorton_t;

typedef struct {
	void *what;
	morton_t where;
} map_range_t;

static const morton_t m1 = 0x111111111111ULL;
static const morton_t m0 = 0x800000000000ULL;

static inline uint16_t
morton_unsign(int16_t n)
{
	uint16_t r = ((smorton_t) n + SHRT_MAX);

	if (r == USHRT_MAX)
		return 1;

	return r;
}

morton_t
pos_morton(int16_t *p, uint8_t dim)
{
	uint16_t up[dim];
	morton_t res = 0;
	int i, j;

	for (i = 0; i < dim; i++)
		up[i] = morton_unsign(p[i]);

	for (i = 0; i < 16; i ++) {
		for (j = 0; j < dim; j++)
			res |= ((morton_t) ((up[j] >> i) & 1)) << (j + (dim * i));
	}

	return res;
}

static inline int16_t
morton_sign(uint16_t n)
{
	return (smorton_t) n - SHRT_MAX;
}

void
morton_pos(int16_t *p, morton_t code, uint8_t dim)
{
	morton_t up[dim];
	int i, j;

	memset(up, 0, sizeof(up));

	for (i = 0; i < 16; i ++) {
		for (j = 0; j < dim; j++)
			up[j] |= ((code >> (j + dim * i)) & 1) << i;
	}

	for (j = 0; j < dim; j++)
		p[j] = morton_sign(up[j]);
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
sdb_inrange(morton_t dr, int16_t *min, int16_t *max, unsigned char dim)
{
	int16_t drp[dim];

	// TODO use ffs
	morton_pos(drp, dr, dim);

	for (int i = 0; i < dim; i++)
		if (drp[i] < min[i] || drp[i] >= max[i])
			return 0;

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
sdb_range_unsafe(map_range_t *res, sdb_t *sdb, size_t n, int16_t *min, int16_t *max)
{
	morton_t rmin = pos_morton(min, sdb->dim),
		 rmax = pos_morton(max, sdb->dim),
		 lm = 0,
		 code;

	map_range_t *r = res;
	DBC *cur;
	DBT key, pkey, data;
	size_t nn = n;
	int ret = 0;
	int dbflags;

	if (sdb->point2ptr->cursor(sdb->point2ptr, NULL, &cur, 0))
		return -1;

	memset(&data, 0, sizeof(DBT));
	memset(&pkey, 0, sizeof(DBT));

	dbflags = DB_SET_RANGE;
	memset(&key, 0, sizeof(DBT));
	key.data = &rmin;
	key.size = sizeof(rmin);

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

	if (code > rmax)
		goto out;

	if (sdb_inrange(code, min, max, sdb->dim)) {
		r->what = pkey.data;
		r->where = code;
		r++;
		nn--;
		if (nn == 0)
			goto out;
	} else
		sdb_compute_bmlm(&rmin, &lm, code, rmin, rmax);

	goto next;

out:	if (cur->close(cur) || ret)
		return -1;

	return r - res;
}

/* this version accounts for type limits,
 * and wraps around them in each direction, if necessary
 * TODO iterative form
 */

static int
sdb_range_safe(map_range_t *res, sdb_t *sdb, size_t n, int16_t *min, int16_t *max, int dim)
{
	map_range_t *re = res;
	int i, aux;
	size_t nn = n;
	int16_t lmin[dim], lmax[dim];
	memcpy(lmin, min, sizeof(int16_t) * dim);
	memcpy(lmax, max, sizeof(int16_t) * dim);

	for (i = dim; i < sdb->dim; i++) {
		// FIXME account for world limits (might be different from data type limits)

		if (min[i] <= max[i])
			continue;

		lmin[i] = max[i];
		lmax[i] = SHRT_MAX;
		aux = sdb_range_safe(re, sdb, nn, min, lmax, i + 1);

		if (aux < 0)
			return -1;

		nn -= aux;
		re += aux;

		lmax[i] = min[i];
		lmin[i] = SHRT_MIN;
		aux = sdb_range_safe(re, sdb, nn, lmin, lmax, i + 1); 

		if (aux < 0)
			return -1;

		nn -= aux;

		return n - nn;
	}

	return sdb_range_unsafe(re, sdb, nn, min, max);
}

static inline size_t m_compute(unsigned char dim, int16_t *min, int16_t *max) {
	register size_t m = 1;

	for (int i = 0; i < dim; i++)
		m *= max[i] < min[i]
			? (max[i] - SHRT_MIN) + (SHRT_MAX - min[i])
			: max[i] - min[i];

	return m;
}

morton_t
point_rel_idx(int16_t *p, int16_t *min, int16_t *max, uint8_t dim)
{
	morton_t res = 0;
	int i, j;

	for (i = 0; i < dim; i++) {
		morton_t lres = p[i] < min[i]
			? (p[i] - SHRT_MIN) + (SHRT_MAX - min[i])
			: (p[i] - min[i]);

		for (j = 0; j < i; j++)
			lres *= max[j] < min[j]
				? (max[j] - SHRT_MIN) + (SHRT_MAX - min[j])
				: (max[j] - min[j]);

		res += lres;
	}

	return res;
}

void
sdb_search(void **mat, sdb_t *sdb, int16_t *min, int16_t *max)
{
	unsigned m = m_compute(sdb->dim, min, max);
	map_range_t buf[m];
	size_t n;
	int i;

	n = sdb_range_safe(buf, sdb, m, min, max, 0);

	for (i = 0; i < m; i++)
		mat[i] = NULL;

	for (i = 0; i < n; i++) {
		int16_t p[sdb->dim];
		morton_pos(p, buf[i].where, sdb->dim);
		mat[point_rel_idx(p, min, max, sdb->dim)] = buf[i].what;
	}
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
map_close(sdb_t *sdb)
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
	CBUG(ret);
}

void
map_put(sdb_t *sdb, int16_t *p, void *thing, int flags)
{
	morton_t code = pos_morton(p, sdb->dim);
	_sdb_put(sdb, code, thing, flags);
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
