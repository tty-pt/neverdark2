#ifndef MODEL_H
#define MODEL_H
#include "cgltf/cgltf.h"

struct model {
	cgltf_data *gltf;
	int dl;
	/* vec3 position; */
	/* versor rotation; */
};

int model_load(struct model *m, char *fname);

#endif
