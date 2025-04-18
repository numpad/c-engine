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

static void draw_node(model_t *model, shader_t *shader, cgltf_node *node, mat4 modelmatrix, model_skeleton_t *skeleton);
static void interpolate_vec3(cgltf_animation_sampler *sampler, float time, vec3 dest);
static void interpolate_quat(cgltf_animation_sampler *sampler, float time, versor dest);

static void swap_node_transforms(cgltf_node *original, skeleton_joint_t *actual);
static float calculate_animation_duration(model_t *, usize animation_index);

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

	// skin
	model->inverse_bind_matrices = NULL;
	model->skin                  = NULL;
	assert(data->skins_count <= 1 && "Multiple skins are not supported");
	if (data->skins_count == 1) {
		model->skin                  = &data->skins[0];
		assert(model->skin->joints_count > 0 && "We can expect one bone at least, right!?");
		assert(model->skin->joints_count <= 48 && "More than 48 joints are not supported!"); // Need to change the model vertex shader if this number changes.
		model->inverse_bind_matrices = malloc(sizeof(mat4)        * model->skin->joints_count);
		model->joint_nodes           = malloc(sizeof(cgltf_node*) * model->skin->joints_count);
		// load IBMs & bones/joint nodes
		if (model->skin->inverse_bind_matrices) {
			assert(model->skin->inverse_bind_matrices->count == model->skin->joints_count && "Number of inverse bind matrices do not match number of joints.");
			uchar *ibm_ptr = (uchar *)get_accessor_data(model->skin->inverse_bind_matrices);
			for (usize i = 0; i < model->skin->joints_count; ++i) {
				glm_mat4_make((float*)(ibm_ptr + i * sizeof(mat4)), model->inverse_bind_matrices[i]);
			}
		} else {
			for (usize i = 0; i < model->skin->joints_count; ++i) {
				glm_mat4_identity(model->inverse_bind_matrices[i]);
			}
		}
		for (usize i = 0; i < model->skin->joints_count; ++i) {
			model->joint_nodes[i] = model->skin->joints[i];
		}
	}

	glBindVertexArray(0);
	return 0;
}

void model_draw(model_t *model, shader_t *shader, struct camera *camera, mat4 modelmatrix, model_skeleton_t *skeleton) {
	assert(model != NULL);
	assert(shader != NULL && shader->kind == SHADER_KIND_MODEL);
	assert(camera != NULL);
	assert(skeleton == NULL || skeleton->model == model);

	glBindVertexArray(model->vao);
	shader_use(shader);
	glBindBuffer(GL_ARRAY_BUFFER, model->vertex_buffers[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->index_buffers[0]);
	shader_set_texture(shader, shader->uniforms.model.diffuse,    GL_TEXTURE0, &model->texture0);
	shader_set_mat4(shader,    shader->uniforms.model.projection, (float*)&camera->projection);
	shader_set_mat4(shader,    shader->uniforms.model.view,       (float*)&camera->view);
	if (skeleton) {
		// TODO: Reset when skeleton is NULL?
		shader_set_mat4_ptr(shader, shader->uniforms.model.bone_transforms, model->skin->joints_count, (float*)skeleton->final_joint_matrices);
	}

	cgltf_scene *scene = model->gltf_data->scene;
	assert(scene != NULL);
	for (usize i = 0; i < scene->nodes_count; ++i) {
		draw_node(model, shader, scene->nodes[i], modelmatrix, skeleton);
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void model_destroy(model_t *model) {
	assert(model != NULL);
	if (model->skin != NULL) {
		free(model->inverse_bind_matrices);
		free(model->joint_nodes);
	}
	glDeleteBuffers(model->gltf_data->buffers_count, model->vertex_buffers);
	glDeleteBuffers(model->gltf_data->buffers_count, model->index_buffers);
	cgltf_free(model->gltf_data);
	glDeleteVertexArrays(1, &model->vao);
	texture_destroy(&model->texture0);
}

// Skeleton

void model_skeleton_init_from_model(model_skeleton_t *skeleton, const model_t *model) {
	assert(skeleton != NULL);
	assert(model != NULL && model->gltf_data != NULL);

	skeleton->model                 = model;
	skeleton->animation_index       = MODEL_ANIMATION_NONE;
	skeleton->joint_node_transforms = calloc(model->gltf_data->nodes_count, sizeof(skeleton_joint_t));
	skeleton->final_joint_matrices  = malloc(model->skin->joints_count * sizeof(mat4));

	for (usize i = 0; i < model->gltf_data->nodes_count; ++i) {
		cgltf_node       *node             = &model->gltf_data->nodes[i];
		usize             joint_node_index = cgltf_node_index(skeleton->model->gltf_data, node);
		skeleton_joint_t *joint            = &skeleton->joint_node_transforms[joint_node_index];

		memcpy(&joint->matrix,      node->matrix,      sizeof(mat4));
		memcpy(&joint->translation, node->translation, sizeof(vec3));
		memcpy(&joint->rotation,    node->rotation,    sizeof(versor));
		memcpy(&joint->scale,       node->scale,       sizeof(vec3));
	}

	// Set the initial joint matrices.
	for (usize i = 0; i < skeleton->model->skin->joints_count; ++i) {
		mat4 global;
		cgltf_node_transform_world(skeleton->model->joint_nodes[i], (float*)global);
		glm_mat4_mul(global, skeleton->model->inverse_bind_matrices[i], skeleton->final_joint_matrices[i]);
	}
}

void model_skeleton_destroy(model_skeleton_t *skeleton) {
	assert(skeleton != NULL);
	free(skeleton->final_joint_matrices);
	free(skeleton->joint_node_transforms);
}

void model_skeleton_animate(model_skeleton_t *skeleton, float time) {
	assert(skeleton != NULL);

	// TODO: Do we want to allow calling with no animation?
	//       Pros: easier to handle
	//       Cons: a bit too implicit?
	assert(skeleton->animation_index != MODEL_ANIMATION_NONE);
	assert(skeleton->model != NULL);
	assert(skeleton->model->gltf_data != NULL);
	assert(skeleton->model->gltf_data->animations_count > 0 && "Need at least one animation.");
	assert(skeleton->animation_index < skeleton->model->gltf_data->animations_count);
	cgltf_animation *anim = &skeleton->model->gltf_data->animations[skeleton->animation_index];
	for (usize i = 0; i < anim->channels_count; ++i) {
		cgltf_animation_channel *channel          = &anim->channels[i];
		cgltf_animation_sampler *sampler          = channel->sampler;
		cgltf_node              *node             = channel->target_node;
		usize                    joint_node_index = cgltf_node_index(skeleton->model->gltf_data, node);
		skeleton_joint_t        *joint            = &skeleton->joint_node_transforms[joint_node_index];
		switch (channel->target_path) {
			case cgltf_animation_path_type_translation:
				interpolate_vec3(sampler, time, joint->translation);
				break;
			case cgltf_animation_path_type_rotation:
				interpolate_quat(sampler, time, joint->rotation);
				break;
			case cgltf_animation_path_type_scale:
				interpolate_vec3(sampler, time, joint->scale);
				break;
			case cgltf_animation_path_type_invalid:
			case cgltf_animation_path_type_weights:
			case cgltf_animation_path_type_max_enum:
				assert(0 && "this animation path type is not implemented!");
				break;
		}
	}

	// The logic here is pretty meh, the main reason for this is that I
	// don't want to recreate the joint/bone hierarchy in the skeleton,
	// as cgltf already provides this. And I'm a bit lazy atm.
	// cgltf also provides the cgltf_node_transform_local()/-_world()
	// functions.
	// In the future I'd like to fully control the hierarchy myself,
	// the skeleton should probably also store the bone hierarchy in
	// BFS order, so I can later just iterate over it and calculate
	// the transformations. This would allow me to just use
	// _node_transform_local() like draw_node() does, making less
	// duplicate/unnecessary mat4 computations.
	//
	// For now this is sufficient.
	// The gist is that our ACTUAL bone & node transformations
	// are stored in the skeleton->joint_node_transforms.
	// Now, when I calculate the final mat4's, we copy the ACTUAL
	// transformations into the cgltf node, as we rely on their
	// node->parent relationship in _node_transform_world().
	// After the mat4's are calculated, we write the original values back into
	// the nodes, and the ACTUAL values back into our transformations.

	// move skeleton transforms into gltf hierarchy
	const usize nodes_count = skeleton->model->gltf_data->nodes_count;
	for (usize i = 0; i < nodes_count; ++i) {
		cgltf_node *joint_node = &skeleton->model->gltf_data->nodes[i];
		usize joint_node_index = cgltf_node_index(skeleton->model->gltf_data, joint_node);
		skeleton_joint_t *joint = &skeleton->joint_node_transforms[joint_node_index];
		swap_node_transforms(joint_node, joint);
	}
	// calculate final transforms for joints
	for (usize i = 0; i < skeleton->model->skin->joints_count; ++i) {
		cgltf_node *joint_node = skeleton->model->skin->joints[i];
		mat4 global;
		cgltf_node_transform_world(joint_node, (float*)global);
		glm_mat4_mul(global, skeleton->model->inverse_bind_matrices[i], skeleton->final_joint_matrices[i]);
	}
	// restore transforms
	for (usize i = 0; i < nodes_count; ++i) {
		cgltf_node *joint_node = &skeleton->model->gltf_data->nodes[i];
		usize joint_node_index = cgltf_node_index(skeleton->model->gltf_data, joint_node);
		skeleton_joint_t *joint = &skeleton->joint_node_transforms[joint_node_index];
		swap_node_transforms(joint_node, joint);
	}
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

static void draw_node(model_t *model, shader_t *shader, cgltf_node *node, mat4 parent_transform, model_skeleton_t *skeleton) {
	mat4 global_transform;
	if (skeleton) {
		// With a skeleton we need to use our custom node transforms,
		// without one we just draw the model as is.
		usize joint_node_index = cgltf_node_index(model->gltf_data, node);
		skeleton_joint_t *joint = &skeleton->joint_node_transforms[joint_node_index];
		swap_node_transforms(node, joint);
		cgltf_node_transform_local(node, (float*)global_transform);
		swap_node_transforms(node, joint);
	} else {
		cgltf_node_transform_local(node, (float*)global_transform);
	}
	glm_mat4_mul(parent_transform, global_transform, global_transform);

	shader_set_mat4(shader, shader->uniforms.model.model, (float*)global_transform);
	shader_set_float(shader, shader->uniforms.model.is_rigged, (node->skin && skeleton) ? 1 : 0);

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
		draw_node(model, shader, child, global_transform, skeleton);
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

static void swap_node_transforms(cgltf_node *original, skeleton_joint_t *actual) {
	memswap(actual->matrix,      original->matrix,      sizeof(mat4));
	memswap(actual->translation, original->translation, sizeof(vec3));
	memswap(actual->rotation,    original->rotation,    sizeof(versor));
	memswap(actual->scale,       original->scale,       sizeof(vec3));
}

static float calculate_animation_duration(model_t *model, usize animation_index) {
	assert(model != NULL);
	assert(animation_index != MODEL_ANIMATION_NONE);
	assert(animation_index < model->gltf_data->animations_count);

	float max_keyframe = -1.0f;
	cgltf_animation *anim = &model->gltf_data->animations[animation_index];
	for (usize i = 0; i < anim->channels_count; ++i) {
		cgltf_animation_channel *channel = &anim->channels[i];
		cgltf_animation_sampler *sampler = channel->sampler;
		float *keyframes                 = get_accessor_data(sampler->input);
		usize keyframes_count            = sampler->input->count;
		assert(keyframes[0] < 0.0001f && "We assume all animations start at keyframe 0.0s, but this is probably not important?");
		max_keyframe = fmaxf(max_keyframe, keyframes[keyframes_count - 1]);
	}
	return max_keyframe;
}

