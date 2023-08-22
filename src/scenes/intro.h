#ifndef INTRO_H
#define INTRO_H

#include "scene.h"
#include "gl/vbuffer.h"

struct engine_s;

struct intro_s {
	struct scene_s base;

	float timer;
	float time_passed, time_passed_max;

	int logo_image_nvg;
};

void intro_init(struct intro_s *intro, struct engine_s *engine);

#endif

