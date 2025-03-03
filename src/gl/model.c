#include "model.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <SDL_opengles2.h>
#include <cglm/cglm.h>
#include <cglm/mat4.h>
#include "util/util.h"
#include "util/str.h"
#include "gl/camera.h"
#include "gl/shader.h"

#define MODEL_ANIMATION_NONE ((usize)-1)

///////////////
//  STRUCTS  //
///////////////

//////////////
//  STATIC  //
//////////////

static void *get_accessor_data(cgltf_accessor *);
static GLint accessor_to_component_size(cgltf_accessor *access);
static const char* accessor_to_component_size_name(cgltf_accessor *access);
static const char* attribute_type_to_name(cgltf_attribute_type type);
static GLint accessor_to_component_type(cgltf_accessor *access);
static const char *accessor_to_component_type_name(cgltf_accessor *access);

static void draw_node(model_t *model, shader_t *shader, cgltf_node *node, mat4 modelmatrix);
static void print_debug_info(cgltf_data *data);

static void update_bone_matrices(model_t *model);

static void interpolate_vec3(cgltf_animation_sampler *sampler, float time, vec3 dest);
static void interpolate_quat(cgltf_animation_sampler *sampler, float time, versor dest);

//////////////
//  PUBLIC  //
//////////////

int model_init_from_file(model_t *model, const char *path) {
	// TODO: This needs an error return value, and handle error cases...
	// TODO: lets maybe free the cgltf_data here?
	assert(model != NULL);
	assert(path != NULL);
	assert(sizeof(mat4) == (16 * sizeof(float))); // General assumption...

	model->gltf_data             = NULL;
	model->joint_count           = 0;
	model->joint_nodes           = NULL;
	model->inverse_bind_matrices = NULL;
	model->final_joint_matrices  = NULL;
	model->animation_index       = MODEL_ANIMATION_NONE;
	model->is_animated           = 0;
	glGenVertexArrays(1, &model->vao);

	cgltf_options options = {0};
	cgltf_data *data = NULL;
	cgltf_result result = cgltf_parse_file(&options, path, &data);
	if (result != cgltf_result_success) {
		fprintf(stderr, "[warn] couldn't load model from \"%s\" (error=%d)...\n", path, result);
		return 1;
	}

	model->gltf_data = data;

	result = cgltf_load_buffers(&options, data, path);
	if (result != cgltf_result_success) {
		fprintf(stderr, "[warn] couldn't load buffers from \"%s\"...\n", path);
		return 1;
	}

	glBindVertexArray(model->vao);
	// load buffer data
	assert(data->buffers_count < count_of(model->vertex_buffers));
	assert(data->buffers_count == 1 && "multiple buffers are not tested, i guess rendering doesnt handle them either?");
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
		settings.flip_y = 0;
		if (texture_view->texture != NULL) {
			settings.filter_min = texture_view->texture->sampler->min_filter;
			settings.filter_mag = texture_view->texture->sampler->mag_filter;
			settings.wrap_s     = texture_view->texture->sampler->wrap_s;
			settings.wrap_t     = texture_view->texture->sampler->wrap_t;
			if (UTIL_IS_GL_FILTER_MIPMAP(settings.filter_min) || UTIL_IS_GL_FILTER_MIPMAP(settings.filter_mag)) {
				settings.gen_mipmap = 1;
			}
		}

		if (texture_view->texture != NULL && texture_view->texture->image->uri != NULL) {
			const char *relative_image_path = texture_view->texture->image->uri;
			char image_path[256];
			str_path_replace_filename(path, relative_image_path, 256, image_path);
			// TODO: Let's cache this, needs an asset manager.
			texture_init_from_image(&model->texture0, image_path, &settings);
		} else if (i < data->images_count) {
			unsigned int image_data_len = data->images[i].buffer_view->size;
			unsigned char *image_data = ((uchar *)data->images[i].buffer_view->buffer->data + data->images[i].buffer_view->offset);
			texture_init_from_memory(&model->texture0, image_data_len, image_data, &settings);
		}
	}

	assert(data->skins_count <= 1 && "Cannot handle multiple skins...");

	if (data->skins_count > 0) {
		cgltf_skin *skin             = &data->skins[0];
		model->is_animated           = 1;
		model->joint_count           = skin->joints_count;
		assert(model->joint_count < 48 && "model vertex shader has 48 joints hardcoded!");
		model->joint_nodes           = malloc(sizeof(cgltf_node*) * model->joint_count);
		model->inverse_bind_matrices = malloc(sizeof(mat4)        * model->joint_count);
		model->final_joint_matrices  = malloc(sizeof(mat4)        * model->joint_count);

		if (skin->inverse_bind_matrices) {
			uchar *ibm_ptr = (uchar *)get_accessor_data(skin->inverse_bind_matrices);
			for (usize i = 0; i < model->joint_count; ++i) {
				glm_mat4_make((float*)(ibm_ptr + i * sizeof(mat4)), model->inverse_bind_matrices[i]);
			}
		} else {
			for (usize i = 0; i < model->joint_count; ++i) {
				glm_mat4_identity(model->inverse_bind_matrices[i]);
			}
		}

		for (usize i = 0; i < model->joint_count; ++i) {
			model->joint_nodes[i] = skin->joints[i];
		}

		update_bone_matrices(model);
	}

	glBindVertexArray(0);
	return 0;
}

void model_update_animation(model_t *model, float time) {
	assert(model != NULL);
	assert(model->gltf_data != NULL);
	assert(model->gltf_data->animations_count > 0 && "Need at least one animation.");
	assert(model->animation_index != MODEL_ANIMATION_NONE);

	cgltf_animation *anim = &model->gltf_data->animations[model->animation_index];
	for (size_t i = 0; i < anim->channels_count; ++i) {
		cgltf_animation_channel *channel = &anim->channels[i];
		cgltf_animation_sampler *sampler = channel->sampler;
		cgltf_node              *node    = channel->target_node;
		switch (channel->target_path) {
			case cgltf_animation_path_type_translation: {
				vec3 trans;
				interpolate_vec3(sampler, time, trans);
				memcpy(node->translation, &trans, sizeof(trans));
				break;
			}
			case cgltf_animation_path_type_rotation: {
				versor rot;
				interpolate_quat(sampler, time, rot);
				memcpy(node->rotation, &rot, sizeof(rot));
				break;
			}
			case cgltf_animation_path_type_scale: {
				vec3 scale;
				interpolate_vec3(sampler, time, scale);
				memcpy(node->scale, &scale, sizeof(scale));
				break;
			}
			case cgltf_animation_path_type_invalid:
			case cgltf_animation_path_type_weights:
			case cgltf_animation_path_type_max_enum:
				assert(0 && "this animation path type is not implemented!");
				break;
		}
	}

	update_bone_matrices(model);
}

void model_draw(model_t *model, shader_t *shader, struct camera *camera, mat4 modelmatrix) {
	assert(model != NULL);

	glBindVertexArray(model->vao);
	shader_use(shader);
	glBindBuffer(GL_ARRAY_BUFFER, model->vertex_buffers[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->index_buffers[0]);
	shader_set_uniform_texture(shader, "u_diffuse",    GL_TEXTURE0, &model->texture0);
	shader_set_uniform_mat4(shader,    "u_projection", (float*)&camera->projection);
	shader_set_uniform_mat4(shader,    "u_view",       (float*)&camera->view);
	if (model->is_animated) {
		GLint u_bone_transforms = glGetUniformLocation(shader->program, "u_bone_transforms");
		glUniformMatrix4fv(u_bone_transforms, model->joint_count, GL_FALSE, (const GLfloat *)model->final_joint_matrices);
	}

	cgltf_scene *scene = model->gltf_data->scene;
	assert(scene != NULL);
	for (usize i = 0; i < scene->nodes_count; ++i) {
		draw_node(model, shader, scene->nodes[i], modelmatrix);
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


void model_destroy(model_t *model) {
	assert(model != NULL);

	if (model->is_animated == 1) {
		free(model->joint_nodes);
		free(model->inverse_bind_matrices);
		free(model->final_joint_matrices);
	}

	glDeleteBuffers(model->gltf_data->buffers_count, model->vertex_buffers);
	glDeleteBuffers(model->gltf_data->buffers_count, model->index_buffers);
	cgltf_free(model->gltf_data);
	glDeleteVertexArrays(1, &model->vao);

	texture_destroy(&model->texture0);
}

void model_set_node_hidden(model_t *model, const char *name, int is_hidden) {
	assert(model != NULL);
	assert(name != NULL);

#ifdef DEBUG
	int node_with_name_exists = 0;
#endif

	for (cgltf_size node_index = 0; node_index < model->gltf_data->nodes_count; ++node_index) {
	}

#ifdef DEBUG
	assert(node_with_name_exists == 1);
#endif
}


////////////
// STATIC //
////////////

static void *get_accessor_data(cgltf_accessor *accessor) {
	return (void*)((uchar*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset);
}

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

static const char* attribute_type_to_name(cgltf_attribute_type type) {
	switch (type) {
		case cgltf_attribute_type_position: return "position";
		case cgltf_attribute_type_normal  : return "normal";
		case cgltf_attribute_type_color   : return "color";
		case cgltf_attribute_type_texcoord: return "texcoord";
		case cgltf_attribute_type_joints  : return "joints";
		case cgltf_attribute_type_tangent : return "tangent";
		case cgltf_attribute_type_weights : return "weights";
		case cgltf_attribute_type_custom  : return "custom";
		case cgltf_attribute_type_invalid : return "invalid";
		case cgltf_attribute_type_max_enum: return "invalid (MAX_ENUM)";
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

static const char *accessor_to_component_type_name(cgltf_accessor *access) {
	const char *name = NULL;
	switch(access->component_type) {
		case cgltf_component_type_r_8     : name = "GL_BYTE";           break;
		case cgltf_component_type_r_8u    : name = "GL_UNSIGNED_BYTE";  break;
		case cgltf_component_type_r_16    : name = "GL_SHORT";          break;
		case cgltf_component_type_r_16u   : name = "GL_UNSIGNED_SHORT"; break;
		case cgltf_component_type_r_32u   : name = "GL_UNSIGNED_INT";   break;
		case cgltf_component_type_r_32f   : name = "GL_FLOAT";          break;
		case cgltf_component_type_invalid :
		case cgltf_component_type_max_enum:
			break;
	}
	assert(name != NULL);
	return name;
}

static void draw_node(model_t *model, shader_t *shader, cgltf_node *node, mat4 parent_transform) {
	mat4 global_transform;
	cgltf_node_transform_local(node, (float*)global_transform);
	glm_mat4_mul(parent_transform, global_transform, global_transform);

	shader_set_uniform_mat4(shader, "u_model", (float*)global_transform);
	shader_set_uniform_float(shader, "u_is_rigged", node->skin ? 1 : 0);

	if (node->mesh) {
		cgltf_mesh *mesh = node->mesh;

		for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; ++prim_index) {
			cgltf_primitive *primitive = &mesh->primitives[prim_index];
			// set attributes
			for (cgltf_size attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index) {
				cgltf_attribute *attrib = &primitive->attributes[attrib_index];
				cgltf_accessor *access = attrib->data;
				// TODO: Whats up with access->count
				assert(access->is_sparse == 0);
				const int location = glGetAttribLocation(shader->program, attrib->name);
				if (location < 0) {
					continue;
				}
				assert(access->offset == 0 && "This isn't handled at the moment...");
				assert(access->stride != 0 && "stride=0 is not supported");
				assert( (access->buffer_view->stride == 0 || access->buffer_view->stride == access->stride)
						&& "No idea how to handle different strides");
				glEnableVertexAttribArray(location);
				switch (access->component_type) {
				case cgltf_component_type_r_8u:
					// Sadly(?), glVertexAttrib_I_Pointer() doesn't work on mobile,
					// but it is completely fine to implicitly convert to float.
					// So just fall-through here:
				case cgltf_component_type_r_32f:
					glVertexAttribPointer(location, accessor_to_component_size(access), accessor_to_component_type(access),
						access->normalized, access->stride, (void *)access->buffer_view->offset);
					break;
				case cgltf_component_type_r_8:
				case cgltf_component_type_r_16:
				case cgltf_component_type_r_16u:
				case cgltf_component_type_r_32u:
				case cgltf_component_type_invalid:
				case cgltf_component_type_max_enum:
					assert(0 && "component type not supported!");
					break;
				}
			}

			assert(primitive->indices != NULL && "only indexed drawing is supported, implement glDrawArrays!");
			assert(primitive->indices->offset == 0 && "need to consider this offset");
			glDrawElements(GL_TRIANGLES, primitive->indices->count, accessor_to_component_type(primitive->indices), (void*)primitive->indices->buffer_view->offset);
			// cleanup
			for (cgltf_size attrib_index = 0; attrib_index < primitive->attributes_count; ++attrib_index) {
				char *attrib_name = primitive->attributes[attrib_index].name;
				const int location = glGetAttribLocation(shader->program, attrib_name);
				if (location >= 0) {
					glDisableVertexAttribArray(location);
				}
			}
		}
	}

	for (cgltf_size child_index = 0; child_index < node->children_count; ++child_index) {
		cgltf_node *child = node->children[child_index];
		draw_node(model, shader, child, global_transform);
	}
}

static void print_debug_info(cgltf_data *data) {
	printf("-------------------\n");
	// find images
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

	// animations
	for (cgltf_size skin_index = 0; skin_index < data->skins_count; ++skin_index) {
		cgltf_skin *skin = &data->skins[skin_index];
		printf("Skin: %s\n", skin->name);
	}
	printf("Animations: %ld\n", data->animations_count);
	for (cgltf_size anim_index = 0; anim_index < data->animations_count; ++anim_index) {
		cgltf_animation *anim = &data->animations[anim_index];
		printf(" - %s: %ld channels, %ld samplers\n", anim->name, anim->channels_count, anim->samplers_count);
	}
}

static void update_bone_matrices(model_t *model) {
	for (usize i = 0; i < model->joint_count; ++i) {
		mat4 global = GLM_MAT4_IDENTITY_INIT;
		cgltf_node_transform_world(model->joint_nodes[i], (float*)global);
		glm_mat4_mul(global, model->inverse_bind_matrices[i], model->final_joint_matrices[i]);
	}
}

static void interpolate_vec3(cgltf_animation_sampler *sampler, float time, vec3 dest) {
	assert(sampler != NULL);
	// inputs
	assert(sampler->input->component_type == cgltf_component_type_r_32f);
	assert(sampler->input->has_min && sampler->input->has_max);
	assert(sampler->input->is_sparse      == 0);
	assert(sampler->input->normalized     == 0);
	assert(sampler->input->type           == cgltf_type_scalar);
	// outputs
	assert(sampler->output->component_type == cgltf_component_type_r_32f);
	assert(sampler->output->has_max        == 0 && sampler->output->has_min == 0); // output has no min/max?
	assert(sampler->output->is_sparse      == 0);
	assert(sampler->output->normalized     == 0);
	assert(sampler->output->type           == cgltf_type_vec3);
	float *keyframe_times = get_accessor_data(sampler->input);
	usize keyframes_count = sampler->input->count;
	assert(sampler->input->count == sampler->output->count);

	assert(sampler->output->stride > 0);
	int stride = sampler->output->stride;

	if (time <= keyframe_times[0]) {
		float *data = get_accessor_data(sampler->output);
		glm_vec3_make(data, dest);
		return;
	}
	if (time >= keyframe_times[keyframes_count - 1]) {
		float *data = (float *)((uchar *)get_accessor_data(sampler->output) + (keyframes_count - 1) * stride);
		glm_vec3_make(data, dest);
		return;
	}

	usize i;
	for (i = 0; i < keyframes_count -1; ++i) {
		if (time < keyframe_times[i + 1]) {
			break;
		}
	}
	assert(i >= 0 && i < keyframes_count);
	float t0 = keyframe_times[i];
	float t1 = keyframe_times[i + 1];
	float factor = (time - t0) / (t1 - t0);

	float *v0 = (float*)((uchar*)get_accessor_data(sampler->output) + i * stride);
	float *v1 = (float*)((uchar*)get_accessor_data(sampler->output) + (i + 1) * stride);

	vec3 a, b;
	glm_vec3_make(v0, a);
	glm_vec3_make(v1, b);
	assert(sampler->interpolation == cgltf_interpolation_type_linear);
	glm_vec3_lerp(a, b, factor, dest);
}

static void interpolate_quat(cgltf_animation_sampler *sampler, float time, versor dest) {
	assert(sampler != NULL);
	// inputs
	assert(sampler->input->component_type == cgltf_component_type_r_32f);
	assert(sampler->input->has_min && sampler->input->has_max);
	assert(sampler->input->is_sparse == 0);
	assert(sampler->input->normalized == 0);
	assert(sampler->input->type == cgltf_type_scalar);
	// outputs
	assert(sampler->output->component_type == cgltf_component_type_r_32f);
	assert(sampler->output->has_max == 0 && sampler->output->has_min == 0); // output has no min/max?
	assert(sampler->output->is_sparse == 0);
	assert(sampler->output->normalized == 0);
	assert(sampler->output->type == cgltf_type_vec4);
	float *keyframe_times = get_accessor_data(sampler->input);
	usize keyframes_count = sampler->input->count;
	assert(sampler->input->count == sampler->output->count);

	assert(sampler->output->stride > 0);
	int stride = sampler->output->stride;

	if (time <= keyframe_times[0]) {
		float *data = get_accessor_data(sampler->output);
		glm_quat_make(data, dest);
		return;
	}
	if (time >= keyframe_times[keyframes_count - 1]) {
		float *data = (float *)((uchar *)get_accessor_data(sampler->output) + (keyframes_count - 1) * stride);
		glm_quat_make(data, dest);
		return;
	}

	usize i;
	for (i = 0; i < keyframes_count - 1; ++i) {
		if (time < keyframe_times[i + 1]) {
			break;
		}
	}
	assert(i >= 0 && i < keyframes_count);
	float t0 = keyframe_times[i];
	float t1 = keyframe_times[i + 1];
	float factor = (time - t0) / (t1 - t0);

	float *v0 = (float*)((unsigned char*)get_accessor_data(sampler->output) + i * stride);
	float *v1 = (float*)((unsigned char*)get_accessor_data(sampler->output) + (i + 1) * stride);

	versor a, b;
	glm_quat_make(v0, a);
	glm_quat_make(v1, b);
	assert(sampler->interpolation == cgltf_interpolation_type_linear);
	glm_quat_slerp(a, b, factor, dest);
}

