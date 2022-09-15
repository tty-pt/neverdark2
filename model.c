#include "model.h"

#include <GL/freeglut.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#include "cglm/cglm.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "debug.h"

static inline size_t
model_unpackf(cgltf_float **floats_r, cgltf_attribute *attr, int components) {
	cgltf_float *floats = calloc(1, attr->data->count * components * sizeof(float));
	size_t count = cgltf_accessor_unpack_floats(attr->data, NULL, 0);
	cgltf_accessor_unpack_floats(attr->data, (cgltf_float *) floats, count);
	*floats_r = floats;
	return count / components;
}

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
	vec2 *texcoord = NULL, *tc;
	int vertexCount, texcoordCount;
	/* CBUG(if (position_accessor->component_type == cgltf_component_type_r_16u)); */
	for (int i = 0; i < prim->attributes_count; i++) {
		cgltf_attribute *attr = &prim->attributes[i];
		warn("attribute type %u\n", attr->type);
		switch (attr->type) {
		case cgltf_attribute_type_position:
			vertexCount = model_unpackf((cgltf_float **) &position, attr, 3);
			break;
		case cgltf_attribute_type_texcoord:
			texcoordCount = model_unpackf((cgltf_float **) &texcoord, attr, 2);
			break;
		default:
			break;
		}
	}

	warn("vertices: %d, texcoords: %d\n", vertexCount, texcoordCount);
	cgltf_material * mat = prim->material;
	warn("texture %p\n", mat->normal_texture.texture);
	cgltf_buffer_view *tview = mat->pbr_metallic_roughness.base_color_texture.texture->image->buffer_view;
	warn("tview %p\n", tview);
	warn("tview size: %lu, offset: %lu\n", tview->size, tview->offset);

	m->texture.data = stbi_load_from_memory(tview->buffer->data + tview->offset, tview->size, &m->texture.w, &m->texture.h, &m->texture.channels, 3);
	glGenTextures(1, &m->texture.id);
	glBindTexture(GL_TEXTURE_2D, m->texture.id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m->texture.w, m->texture.h, 0, GL_RGB, GL_UNSIGNED_BYTE, m->texture.data);
	/* glGenerateMipmap(GL_TEXTURE_2D); */

	m->dl = glGenLists(1);
	glNewList(m->dl, GL_COMPILE);
	glBegin(GL_TRIANGLES);
	glBindTexture(GL_TEXTURE_2D, m->texture.id);

	for (p = position, tc = texcoord;
	     p < position + vertexCount;
	     p ++, tc ++)
	{
		vec3 vertex;
		vec2 tcc;

		memcpy(&vertex, p, sizeof(vertex));
		memcpy(&tcc, tc, sizeof(tcc));
		glTexCoord2f(tcc[0], tcc[1]);
		glVertex3f(vertex[0], vertex[1], vertex[2]);
	}

	glEnd();
	glEndList();

	return 0;
}

