#ifndef GAME_H
#define GAME_H

#include "scenes/scene.h"
#include "game/terrain.h"

struct engine_s;

struct scene_game_s {
	struct scene_s base;
	
	struct terrain_s terrain;
};


void scene_game_init(struct scene_game_s *scene, struct engine_s *engine, int w, int h);

#endif

