#ifndef SCENE_BRICKBREAKER_H
#define SCENE_BRICKBREAKER_H

#include "scene.h"

struct engine_s;

struct scene_brickbreaker_s {
	struct scene_s base;
};

void scene_brickbreaker_init(struct scene_brickbreaker_s *, struct engine_s *);

#endif
