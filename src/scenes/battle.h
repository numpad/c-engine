#ifndef BATTLE_H
#define BATTLE_H

#include <flecs.h>
#include "scene.h"

struct scene_battle {
	struct scene_s base;
};

void scene_battle_init(struct scene_battle *, struct engine *);

#endif

