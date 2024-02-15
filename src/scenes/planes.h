#ifndef SCENE_PLANES_H
#define SCENE_PLANES_H

#include "scene.h"

struct engine_s;

struct scene_planes_s {
	struct scene_s base;
};

void scene_planes_init(struct scene_planes_s *, struct engine_s *);

#endif

