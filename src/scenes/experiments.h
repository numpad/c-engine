#ifndef SCENE_EXPERIMENTS_H
#define SCENE_EXPERIMENTS_H

#include "scene.h"

struct engine_s;

struct scene_experiments_s {
	struct scene_s base;
};

void scene_experiments_init(struct scene_experiments_s *, struct engine_s *);

#endif

