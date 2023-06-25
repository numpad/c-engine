#ifndef GAME_H
#define GAME_H

#include "scenes/scene.h"
#include "game/terrain.h"

struct engine_s;

struct game_s {
	struct scene_s base;
	
	struct terrain_s terrain;
};


void game_init(struct game_s *scene, struct engine_s *engine, int w, int h);

#endif

