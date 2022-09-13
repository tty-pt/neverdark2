#include "model.h"

#include <GL/freeglut.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#include "cglm/cglm.h"
#include "debug.h"

int
model_load(struct model *m, char *fname) {
	cgltf_options options = {};
	m->gltf = NULL;

	if (cgltf_parse_file(&options, fname, &m->gltf) != cgltf_result_success
	    || cgltf_load_buffers(&options, m->gltf, fname) != cgltf_result_success
	    || cgltf_validate(m->gltf) != cgltf_result_success)
		return 1;

	if (m->gltf->meshes_count < 1) {
		cgltf_free(m->gltf);
		return 1;
	}

	cgltf_mesh *mesh = &m->gltf->meshes[0];

	if (mesh->primitives_count < 1) {
		cgltf_free(m->gltf);
		return 1;
	}

	warn("primitives %lu\n", mesh->primitives_count);
	cgltf_primitive *prim = &mesh->primitives[0];
	/* cgltf_accessor *indices = prim->indices; */
	warn("primitive type %u\n", prim->type);
	vec3 *position = NULL, *p;
	int vertexCount;
	/* CBUG(if (position_accessor->component_type == cgltf_component_type_r_16u)); */
	for (int i = 0; i < prim->attributes_count; i++) {
		cgltf_attribute *attr = &prim->attributes[i];
		warn("attribute type %u\n", attr->type);
		if (attr->type == cgltf_attribute_type_position) {
			position = calloc(1, attr->data->count * 3 * sizeof(float));
			size_t floatsToUnpack = cgltf_accessor_unpack_floats(attr->data, NULL, 0);
			cgltf_accessor_unpack_floats(attr->data, (cgltf_float*) position, floatsToUnpack);
			vertexCount = floatsToUnpack / 3;

		}
	}

	m->dl = glGenLists(1);
	glNewList(m->dl, GL_COMPILE);
	glBegin(GL_TRIANGLES);

	for (p = position;
	     p < position + vertexCount;
	     p ++)
	{
		vec3 vertex;
		memcpy(&vertex, p, sizeof(vertex));
		glVertex3f(vertex[0], vertex[1], vertex[2]);
	}

	glEnd();
	glEndList();

	return 0;
}

