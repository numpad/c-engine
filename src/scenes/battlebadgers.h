#ifndef WORMS_H
#define WORMS_H

#include "scene.h"
#include <flecs.h>

struct battlebadgers_s {
	struct scene_s base;

	int bg_shader;
	unsigned int bg_texture;
	unsigned int bg_vbo;

	ecs_world_t *world;
	ecs_query_t *q_render;
};

void battlebadgers_init(struct battlebadgers_s *, struct engine_s *);

#endif

