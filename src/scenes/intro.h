#ifndef INTRO_H
#define INTRO_H

#include "scene.h"
#include "gl/vbuffer.h"

struct engine;

struct scene_intro_s {
	struct scene_s base;

	float timer;
	float time_passed, time_passed_max;

	int logo_image_nvg;
};

void scene_intro_init(struct scene_intro_s *intro, struct engine *engine);

#endif

