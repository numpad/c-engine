#ifndef INTRO_H
#define INTRO_H

#include "scene.h"

struct engine_s;

struct intro_s {
	struct scene_s base;

	float timer;
};

void intro_init(struct intro_s *intro, struct engine_s *engine);

#endif

