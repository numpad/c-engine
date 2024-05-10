#include "model.h"

#include <stdio.h>
#include <assert.h>
#include <SDL_opengles2.h>
#include <cglm/cglm.h>
#include "util/util.h"

//////////////
//  STATIC  //
//////////////

static GLint accessor_to_component_size(cgltf_accessor *access) {
	GLint num_components = -1;
	switch (access->type) {
		case cgltf_type_scalar  : num_components = 1; break;
		case cgltf_type_vec2    : num_components = 2; break;
		case cgltf_type_vec3    : num_components = 3; break;
		case cgltf_type_vec4    : num_components = 4; break;
		case cgltf_type_mat2    :
		case cgltf_type_mat3    :
		case cgltf_type_mat4    :
			fprintf(stderr, "[warn] cgltf_type %u not handled, unknown number of components!\n", access->type);
		case cgltf_type_invalid :
		case cgltf_type_max_enum:
			break;
	}

	assert(num_components != -1);
	return num_components;
}

static GLint accessor_to_component_type(cgltf_accessor *access) {
	GLint comp_type = -1;
	switch(access->component_type) {
		case cgltf_component_type_r_8     : comp_type = GL_BYTE;           break;
		case cgltf_component_type_r_8u    : comp_type = GL_UNSIGNED_BYTE;  break;
		case cgltf_component_type_r_16    : comp_type = GL_SHORT;          break;
		case cgltf_component_type_r_16u   : comp_type = GL_UNSIGNED_SHORT; break;
		case cgltf_component_type_r_32u   : comp_type = GL_UNSIGNED_INT;   break;
		case cgltf_component_type_r_32f   : comp_type = GL_FLOAT;          break;
		case cgltf_component_type_invalid :
		case cgltf_component_type_max_enum:
			break;
	}

	assert(comp_type != -1);
	return comp_type;
}

//////////////
//  PUBLIC  //
//////////////

void model_from_file(model_t *model, const char *path) {
	assert(model != NULL);
	assert(path != NULL);

	cgltf_options options = {0};
	cgltf_data *data = NULL;
	cgltf_result result = cgltf_parse_file(&options, path, &data);
	if (result != cgltf_result_success) {
		fprintf(stderr, "[warn] couldn't load model from \"%s\" (error=%d)...\n", path, result);
		return;
	}

	model->gltf_data = data;

	result = cgltf_load_buffers(&options, data, path);
	if (result != cgltf_result_success) {
		fprintf(stderr, "[warn] couldn't load buffers from \"%s\"...\n", path);
		return;
	}

	if (false) {
		for (cgltf_size i = 0; i < data->images_count; ++i) {
			printf("Image #%ld:\n", i);
			printf(" - name      : \"%s\"\n", data->images[i].name);
			printf(" - uri       : %s\n", data->images[i].uri);
			printf(" - mime-type : %s\n", data->images[i].mime_type);
		}

		for (cgltf_size i = 0; i < data->textures_count; ++i) {
			printf("Texture #%ld:\n", i);
			printf(" - name : \"%s\"\n", data->textures[i].name);
		}

		for (cgltf_size i = 0; i < data->samplers_count; ++i) {
			printf("Sampler #%ld:\n", i);
			printf(" - name : \"%s\"\n", data->samplers[i].name);
		}

		for (cgltf_size i = 0; i < data->animations_count; ++i) {
			printf("Animation #%ld:\n", i);
			printf(" - name     : \"%s\"\n", data->animations[i].name);
			printf(" - channels : %ld\n", data->animations[i].channels_count);
			printf(" - samplers : %ld\n", data->animations[i].samplers_count);
		}

		for (cgltf_size i = 0; i < data->materials_count; ++i) {
			printf("Material #%ld:\n", i);
			printf(" - name : \"%s\"\n", data->materials[i].name);
		}
	}

	// load buffer data
	assert(data->buffers_count < count_of(model->vertex_buffers));
	glGenBuffers(data->buffers_count, model->vertex_buffers);
	glGenBuffers(data->buffers_count, model->index_buffers);
	for (cgltf_size i = 0; i < data->buffers_count; ++i) {
		glBindBuffer(GL_ARRAY_BUFFER, model->vertex_buffers[i]);
		glBufferData(GL_ARRAY_BUFFER, data->buffers[i].size, data->buffers[i].data, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->index_buffers[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->buffers[i].size, data->buffers[i].data, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		printf("Buffer #%ld:\n", i);
		printf(" - name  : \"%s\"\n", data->buffers[i].name);
		printf(" - uri   : \"%s\"\n", data->buffers[i].uri);
		printf(" - size  : %ld\n", data->buffers[i].size);
		printf(" - @data : %p\n", data->buffers[i].data);
	}

	if (false) {
		for (cgltf_size i = 0; i < data->buffer_views_count; ++i) {
			printf("Buffer-View #%ld:\n", i);
			printf(" - name : \"%s\"\n", data->buffer_views[i].name);
			printf(" - type : %d\n", data->buffer_views[i].type);
			printf(" - offset : %ld\n", data->buffer_views[i].offset);
			printf(" - stride : %ld\n", data->buffer_views[i].stride);
			printf(" - size : %ld\n", data->buffer_views[i].size);
			printf(" - @data : %p\n", data->buffer_views[i].data);
		}

		for (cgltf_size i = 0; i < data->accessors_count; ++i) {
			printf("Accessor #%ld:\n", i);
			printf(" - name     : \"%s\"\n", data->accessors[i].name);
			printf(" - comptype : %u\n", data->accessors[i].component_type);
			printf(" - datatype : %u\n", data->accessors[i].type);
			printf(" - nrmlz'd  : %b\n", data->accessors[i].normalized);
			printf(" - offset   : %zb\n", data->accessors[i].offset);
			printf(" - stride   : %zb\n", data->accessors[i].stride);
		}

		for (cgltf_size i = 0; i < data->meshes_count; ++i) {
			printf("Mesh #%ld:\n", i);
			printf(" - name      : \"%s\"\n", data->meshes[i].name);
			printf(" - primitives: %zu\n", data->meshes[i].primitives_count);
			for (cgltf_size j = 0; j < data->meshes[i].primitives_count; ++j) {
				for (cgltf_size k = 0; k < data->meshes[i].primitives[j].attributes_count; ++k) {
					printf("   > attribute : \"%s\"\n", data->meshes[i].primitives[j].attributes[k].name);
				}
			}
		}
	}

	shader_init_from_dir(&model->shader, "res/shader/model/");
}


void model_draw(model_t *model) {
	assert(model != NULL);

	glUseProgram(model->shader.program);
	glBindBuffer(GL_ARRAY_BUFFER, model->vertex_buffers[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->index_buffers[0]);

	for (cgltf_size mesh_index = 0; mesh_index < model->gltf_data->meshes_count; ++mesh_index) {
		cgltf_mesh *mesh = &model->gltf_data->meshes[mesh_index];

		for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; ++prim_index) {
			cgltf_primitive *primitive = &mesh->primitives[prim_index];

			for (cgltf_size attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index) {
				cgltf_attribute *attrib = &primitive->attributes[attrib_index];
				cgltf_accessor *access = attrib->data;

				const int location = glGetAttribLocation(model->shader.program, attrib->name);
				if (location < 0) {
					fprintf(stderr, "[warn] attribute \"%s\" doesn't exist (or is unused).\n", attrib->name);
					continue;
				}
				glVertexAttribPointer(location, accessor_to_component_size(access), accessor_to_component_type(access),
					access->normalized, access->buffer_view->stride, (void *)access->buffer_view->offset);
				glEnableVertexAttribArray(location);
			}

			if (primitive->indices != NULL) {
				glDrawElements(GL_TRIANGLES, primitive->indices->count, accessor_to_component_type(primitive->indices), (void*)primitive->indices->buffer_view->offset);
			} else {
				// TODO: glDrawArrays
			}

			// cleanup
			for (cgltf_size attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index) {
				cgltf_attribute *attrib = &primitive->attributes[attrib_index];
				const int location = glGetAttribLocation(model->shader.program, attrib->name);
				if (location >= 0) {
					glDisableVertexAttribArray(location);
				}
			}
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


void model_destroy(model_t *model) {
	assert(model != NULL);

	glDeleteBuffers(model->gltf_data->buffers_count, model->vertex_buffers);
	// TODO: delete index buffers
	cgltf_free(model->gltf_data);
	shader_destroy(&model->shader);
}

