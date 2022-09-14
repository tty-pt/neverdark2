#ifndef MODEL_H
#define MODEL_H
#include "cgltf/cgltf.h"

struct texture {
	char *data;
	int w, h, channels;
	unsigned id;
};

struct model {
	cgltf_data *gltf;
	int dl;
	struct texture texture;
	/* vec3 position; */
	/* versor rotation; */
};

int model_load(struct model *m, char *fname);

#endif
