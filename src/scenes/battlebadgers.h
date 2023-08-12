#ifndef WORMS_H
#define WORMS_H

#include "scene.h"
#include <flecs.h>

struct vbuffer_s;
struct isoterrain_s;

struct battlebadgers_s {
	struct scene_s base;

	int bg_shader;
	unsigned int bg_texture;
	struct vbuffer_s *bg_vbuf;

	// game state
	struct isoterrain_s *terrain;

	ecs_world_t *world;
	ecs_query_t *q_render, *q_update_pos;
};

void battlebadgers_init(struct battlebadgers_s *, struct engine_s *);

#endif

