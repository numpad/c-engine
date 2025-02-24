#ifndef MODEL_H
#define MODEL_H

#include <cglm/cglm.h>
#include <cgltf.h>
#include "gl/shader.h"
#include "gl/texture.h"
#include "gl/camera.h"

typedef struct model_s {
	cgltf_data *gltf_data;
	unsigned int vertex_buffers[8];
	unsigned int index_buffers[8];
	texture_t texture0;
	// skeletal animation
	uint       joint_count;
	cgltf_node **joint_nodes;
	mat4       *inverse_bind_matrices;
	mat4       *final_joint_matrices;
	// uniforms
	int u_is_rigged;
} model_t;


int  model_init_from_file(model_t *, const char *path);
void model_destroy(model_t *);

void model_draw(model_t *, shader_t *, struct camera *, mat4 modelmatrix);
void model_update_animation(model_t *);

void model_set_node_hidden(model_t *, const char *name, int hidden);

#endif

