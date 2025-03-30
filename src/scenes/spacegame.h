#ifndef SCENE_spacegame_H
#define SCENE_spacegame_H

#include "scene.h"

struct engine;

struct scene_spacegame_s {
	struct scene_s base;
};

void scene_spacegame_init(struct scene_spacegame_s *, struct engine *);

#endif

