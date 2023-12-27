#ifndef WORMS_H
#define WORMS_H

#include <flecs.h>
#include "scene.h"
#include "gl/texture.h"

struct scene_battle_s {
	struct scene_s base;
};

void scene_battle_init(struct scene_battle_s *, struct engine_s *);

#endif

