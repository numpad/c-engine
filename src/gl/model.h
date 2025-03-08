#ifndef MODEL_H
#define MODEL_H

#include <cglm/cglm.h>
#include <cgltf.h>
#include "gl/shader.h"
#include "gl/texture.h"
#include "gl/camera.h"

#define MODEL_ANIMATION_NONE ((usize)-1)

typedef struct model_s {
	cgltf_data  *gltf_data;
	uint         vertex_buffers[8];
	uint         index_buffers[8];
	texture_t    texture0;
	// skeleton
	cgltf_skin  *skin;
	mat4        *inverse_bind_matrices;
	cgltf_node **joint_nodes;
	// gl
	uint vao;
} model_t;

typedef struct {
	mat4   matrix;
	vec3   translation;
	versor rotation;
	vec3   scale;
} skeleton_joint_t;

typedef struct model_skeleton {
	model_t          *model;
	usize             animation_index;
	mat4             *final_joint_matrices;
	skeleton_joint_t *joint_node_transforms;
} model_skeleton_t;

// model functions
int  model_init_from_file  (model_t *, const char *path);
void model_destroy         (model_t *);
void model_draw            (model_t *, shader_t *, struct camera *, mat4 modelmatrix, model_skeleton_t *skeleton);

// skeleton joint

// skeleton functions
void model_skeleton_init_from_model(model_skeleton_t *, const model_t *);
void model_skeleton_destroy        (model_skeleton_t *);
void model_skeleton_animate        (model_skeleton_t *, float time);

#endif

