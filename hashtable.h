#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <db4/db.h>
#include <string.h>
#include "debug.h"

inline static void
db_init(DB **db, DBTYPE type)
{
	CBUG(db_create(db, NULL, 0));
	CBUG((*db)->open(*db, NULL, NULL, NULL, type, DB_CREATE, 0644));
}

inline static void
db_put(DB *db, void *key, size_t key_len, void *value, size_t value_len)
{
	DBT key_dbt, data_dbt;

	memset(&key_dbt, 0, sizeof(DBT));
	memset(&data_dbt, 0, sizeof(DBT));

	key_dbt.data = key;
	key_dbt.size = key_len;
	data_dbt.data = value;
	data_dbt.size = value_len;

	CBUG(db->put(db, NULL, &key_dbt, &data_dbt, 0));
}

inline static int
db_get(DB *db, void *value, void *key, size_t key_len)
{
	DBT key_dbt, data_dbt;
	int ret;

	memset(&key_dbt, 0, sizeof(DBT));
	memset(&data_dbt, 0, sizeof(DBT));

	key_dbt.data = key;
	key_dbt.size = key_len;

	ret = db->get(db, NULL, &key_dbt, &data_dbt, 0);

	if (ret == DB_NOTFOUND)
		return 1;

	memcpy(value, data_dbt.data, data_dbt.size);
	CBUG(ret);

	return 0;
}

inline static void
db_delete(DB *db, void *key, size_t key_len)
{
	DBT key_dbt;

	memset(&key_dbt, 0, sizeof(DBT));

	key_dbt.data = key;
	key_dbt.size = key_len;

	CBUG(db->del(db, NULL, &key_dbt, 0));
}

inline static void *
vhash_get(DB *db, void *key, size_t key_len)
{
	void *value;

	if (db_get(db, &value, key, key_len))
		return NULL;

	return value;
}

inline static void *
svhash_get(DB *db, char *key) {
	return vhash_get(db, key, strlen(key));
}

inline static void
svhash_put(DB *db, const char *key, void *data)
{
	db_put(db, (void *) key, strlen(key), &data, sizeof(void *));
}

#endif

