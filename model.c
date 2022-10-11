#include "model.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#include "stb/stb_image.h"
#include "cglm/cglm.h"
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
	vec3 *position = NULL;
	vec2 *texcoord = NULL;
	vec3 *normal = NULL;
	int vertexCount;
	/* CBUG(if (position_accessor->component_type == cgltf_component_type_r_16u)); */
	for (int i = 0; i < prim->attributes_count; i++) {
		cgltf_attribute *attr = &prim->attributes[i];
		switch (attr->type) {
		case cgltf_attribute_type_position:
			vertexCount = model_unpackf((cgltf_float **) &position, attr, 3);
			break;
		case cgltf_attribute_type_texcoord:
			model_unpackf((cgltf_float **) &texcoord, attr, 2);
			break;
		case cgltf_attribute_type_normal:
			model_unpackf((cgltf_float **) &normal, attr, 3);
			break;
		default:
			break;
		}
	}

	cgltf_material * mat = prim->material;
	cgltf_texture *tex_r = mat->pbr_metallic_roughness.base_color_texture.texture;

	if (tex_r) {
		cgltf_buffer_view *tview = tex_r->image->buffer_view;
		struct texture tex;

		tex.data = stbi_load_from_memory(tview->buffer->data + tview->offset, tview->size, &tex.w, &tex.h, &tex.channels, 3);
		glActiveTexture(GL_TEXTURE0);
		m->texture_id = glg_texture_load(&tex);
	} else {
		m->texture_id = TEX_INVALID;
	}

	{
		cgl_prebind(&m->gl, 1);
		m->gl.npoints = vertexCount;
		cgl_bind(&m->gl, position, normal, texcoord);
	}

	return 0;
}

void
model_render(struct model *m)
{
	glm_mat4_identity(modelm);
	glUniformMatrix4fv(modelLoc, 1, 0, &modelm[0][0]);
	if (m->texture_id != TEX_INVALID)
		glBindTexture(GL_TEXTURE_2D, m->texture_id);
	glBindVertexArray(m->gl.vao);
	glDrawArrays(GL_TRIANGLES, 0, m->gl.npoints);
	GL_DEBUG("model_render");
}
