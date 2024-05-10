#ifndef MODEL_H
#define MODEL_H

#include <cgltf.h>
#include "gl/shader.h"

typedef struct model_s {
	cgltf_data *gltf_data;
	shader_t shader;
	unsigned int vertex_buffers[8];
	unsigned int index_buffers[8];
} model_t;


void model_from_file(model_t *, const char *path);
void model_destroy(model_t *);

void model_draw(model_t *);

#endif

