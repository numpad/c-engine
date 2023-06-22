#ifndef GAME_H
#define GAME_H

#include "scenes/scene.h"

struct engine_s;

struct game_s {
	struct scene_s base;

	unsigned char *world;
	int world_width, world_height;
};


void game_init(struct game_s *scene, struct engine_s *engine, int w, int h);

#endif

