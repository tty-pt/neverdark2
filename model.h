#ifndef MODEL_H
#define MODEL_H
#include "cgltf/cgltf.h"

#define TEX_INVALID ((unsigned) -1)

struct texture {
	char *data;
	int w, h, channels;
};

struct model {
	cgltf_data *gltf;
	int dl;
	unsigned texture_id;
	/* vec3 position; */
	/* versor rotation; */
};

int model_load(struct model *m, char *fname);

#endif
