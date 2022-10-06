#ifndef MODEL_H
#define MODEL_H
#include "cgltf/cgltf.h"
#include "gl_global.h"

struct model {
	cgltf_data *gltf;
	unsigned texture_id;
	cgl_t gl;
	/* vec3 position; */
	/* versor rotation; */
};

int model_load(struct model *m, char *fname);
void model_render(struct model *m);

#endif
