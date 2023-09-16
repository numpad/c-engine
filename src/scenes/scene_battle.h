#ifndef WORMS_H
#define WORMS_H

#include "scene.h"
#include <flecs.h>

struct vbuffer_s;
struct isoterrain_s;
struct cardrenderer_s;

struct scene_battle_s {
	struct scene_s base;

	int bg_shader;
	unsigned int bg_texture;
	struct vbuffer_s *bg_vbuf;

	// game state
	struct isoterrain_s *terrain;
	struct cardrenderer_s *cardrenderer;

	ecs_world_t *world;
	ecs_query_t *q_handcards;
};

void scene_battle_init(struct scene_battle_s *, struct engine_s *);

#endif
