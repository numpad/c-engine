#include "model.h"

#include <stdio.h>
#include <assert.h>
#include <SDL_opengles2.h>
#include <cglm/cglm.h>
#include "util/util.h"
#include "util/str.h"

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

static const char* accessor_to_component_size_name(cgltf_accessor *access) {
	switch (access->type) {
		case cgltf_type_scalar  : return "cgltf_type_scalar";
		case cgltf_type_vec2    : return "cgltf_type_vec2";
		case cgltf_type_vec3    : return "cgltf_type_vec3";
		case cgltf_type_vec4    : return "cgltf_type_vec4";
		case cgltf_type_mat2    : return "cgltf_type_mat2";
		case cgltf_type_mat3    : return "cgltf_type_mat3";
		case cgltf_type_mat4    : return "cgltf_type_mat4";
		case cgltf_type_invalid : return "cgltf_type_invalid";
		case cgltf_type_max_enum: return "cgltf_type_max_enum";
	}

	return NULL;
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

static void draw_node(model_t *model, cgltf_node *node, mat4 modelmatrix) {
	if (node->mesh != NULL) {
		cgltf_mesh *mesh = node->mesh;

		for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; ++prim_index) {
			cgltf_primitive *primitive = &mesh->primitives[prim_index];

			for (cgltf_size attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index) {
				cgltf_attribute *attrib = &primitive->attributes[attrib_index];
				cgltf_accessor *access = attrib->data;

				const int location = glGetAttribLocation(model->shader.program, attrib->name);
				if (location < 0) {
					continue;
				}

				glVertexAttribPointer(location, accessor_to_component_size(access), accessor_to_component_type(access),
					access->normalized, access->buffer_view->stride, (void *)access->buffer_view->offset);
				glEnableVertexAttribArray(location);
			}

			// calculate matrices
			mat4 node_transform;
			cgltf_node_transform_world(node, (float*)node_transform);
			glm_mat4_mul(modelmatrix, node_transform, node_transform);
			shader_set_uniform_mat4(&model->shader, "u_model", (float*)node_transform);

			if (primitive->indices != NULL) {
				glDrawElements(GL_TRIANGLES, primitive->indices->count, accessor_to_component_type(primitive->indices), (void*)primitive->indices->buffer_view->offset);
			} else {
				// TODO: glDrawArrays
			}

			// cleanup
			for (cgltf_size attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index) {
				char *attrib_name = primitive->attributes[attrib_index].name;
				const int location = glGetAttribLocation(model->shader.program, attrib_name);
				if (location >= 0) {
					glDisableVertexAttribArray(location);
				}
			}
		}
	}

	// TODO: why not draw this? this duplicates everything
	/*
	for (cgltf_size child_index = 0; child_index < node->children_count; ++child_index) {
		cgltf_node *child = node->children[child_index];
		draw_node(model, child, modelmatrix);
	}
	*/
}

//////////////
//  PUBLIC  //
//////////////

void model_init_from_file(model_t *model, const char *path) {
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

	{ // find images
		for (cgltf_size i = 0; i < data->materials_count; ++i) {
			printf("Material #%ld:\n", i);
			printf(" - name : \"%s\"\n", data->materials[i].name);
		}
		for (cgltf_size i = 0; i < data->images_count; ++i) {
			printf("Image #%ld:\n", i);
			printf(" - name      : \"%s\"\n", data->images[i].name);
			printf(" - uri       : %s\n", data->images[i].uri);
			printf(" - mime-type : %s\n", data->images[i].mime_type);
			if (data->images[i].buffer_view) {
				printf(" - size      : %zu\n", data->images[i].buffer_view->size);
			} else {
				printf(" - size      : (no buffer view)\n");
			}
		}

		for (cgltf_size i = 0; i < data->textures_count; ++i) {
			printf("Texture #%ld:\n", i);
			printf(" - name : \"%s\"\n", data->textures[i].name);
		}

		for (cgltf_size i = 0; i < data->samplers_count; ++i) {
			printf("Sampler #%ld:\n", i);
			printf(" - name : \"%s\"\n", data->samplers[i].name);
		}
	}

	{ // animations
		for (cgltf_size skin_index = 0; skin_index < data->skins_count; ++skin_index) {
			cgltf_skin *skin = &data->skins[skin_index];
			printf("Skin: %s\n", skin->name);
		}
		for (cgltf_size anim_index = 0; anim_index < data->animations_count; ++anim_index) {
			cgltf_animation *anim = &data->animations[anim_index];
			printf("Anim: %s\n", anim->name);
		}
	}

	// setup shader
	shader_init_from_dir(&model->shader, "res/shader/model/");

	// load buffer data
	assert(data->buffers_count < count_of(model->vertex_buffers));
	glGenBuffers(data->buffers_count, model->vertex_buffers);
	glGenBuffers(data->buffers_count, model->index_buffers);
	for (cgltf_size i = 0; i < data->buffers_count; ++i) {
		glBindBuffer(GL_ARRAY_BUFFER, model->vertex_buffers[i]);
		glBufferData(GL_ARRAY_BUFFER, data->buffers[i].size, data->buffers[i].data, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->index_buffers[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->buffers[i].size, data->buffers[i].data, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	// load materials
	for (cgltf_size i = 0; i < data->materials_count; ++i) {
		cgltf_material *mat = &data->materials[i];
		assert(mat->has_pbr_metallic_roughness);
		cgltf_texture_view *texture_view = &mat->pbr_metallic_roughness.base_color_texture;

		struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
		settings.flip_y     = 0;
		settings.filter_min = texture_view->texture->sampler->min_filter;
		settings.filter_mag = texture_view->texture->sampler->mag_filter;
		settings.wrap_s     = texture_view->texture->sampler->wrap_s;
		settings.wrap_t     = texture_view->texture->sampler->wrap_t;

		if (UTIL_IS_GL_FILTER_MIPMAP(settings.filter_min) || UTIL_IS_GL_FILTER_MIPMAP(settings.filter_mag)) {
			settings.gen_mipmap = 1;
		}

		if (texture_view->texture->image->uri != NULL) {
			const char *relative_image_path = texture_view->texture->image->uri;
			char image_path[256];
			str_path_replace_filename(path, relative_image_path, 256, image_path);
			fprintf(stderr, "[warn] loading image by uri is still experimental: \"%s\"...\n", image_path);

			// TODO: Let's cache this, needs an asset manager.
			texture_init_from_image(&model->texture0, image_path, &settings);
		} else {
			unsigned int image_data_len = data->images[i].buffer_view->size;
			unsigned char *image_data = (unsigned char *)data->images[i].buffer_view->buffer->data + data->images[i].buffer_view->offset;
			texture_init_from_memory(&model->texture0, image_data_len, image_data, &settings);
		}
	}

#ifndef NDEBUG
	// Perform some validation: unused attributes, ...
	glUseProgram(model->shader.program);
	for (cgltf_size mesh_index = 0; mesh_index < model->gltf_data->meshes_count; ++mesh_index) {
		cgltf_mesh *mesh = &model->gltf_data->meshes[mesh_index];
		for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; ++prim_index) {
			cgltf_primitive *primitive = &mesh->primitives[prim_index];
			for (cgltf_size attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index) {
				cgltf_attribute *attrib = &primitive->attributes[attrib_index];
				const GLint location = glGetAttribLocation(model->shader.program, attrib->name);
				if (location < 0) {
					fprintf(stderr, "[warn] attribute \"%s\" (%s) doesn't exist (or is unused).\n",
							attrib->name, accessor_to_component_size_name(attrib->data));
				}
			}
		}
	}
	glUseProgram(0);
#endif
}

void model_draw(model_t *model, mat4 projection, mat4 view, mat4 modelmatrix) {
	assert(model != NULL);

	glUseProgram(model->shader.program);
	glBindBuffer(GL_ARRAY_BUFFER, model->vertex_buffers[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->index_buffers[0]);
	shader_set_uniform_texture(&model->shader, "u_diffuse", GL_TEXTURE0, &model->texture0);
	shader_set_uniform_mat4(&model->shader, "u_projection", (float*)projection);
	shader_set_uniform_mat4(&model->shader, "u_view", (float*)view);

	// TODO: maybe use a stack based approach instead of doing recursion?
	for (cgltf_size node_index = 0; node_index < model->gltf_data->nodes_count; ++node_index) {
		cgltf_node *node = &model->gltf_data->nodes[node_index];
		draw_node(model, node, modelmatrix);
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

	texture_destroy(&model->texture0);
}

