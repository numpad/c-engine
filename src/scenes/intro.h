#ifndef INTRO_H
#define INTRO_H

#include "scene.h"

struct engine_s;

struct intro_s {
	struct scene_s base;

	float timer;
};

struct intro_s *intro_new(struct engine_s *engine);
void intro_destroy(struct scene_s *scene, struct engine_s *engine);
void intro_load(struct scene_s *scene, struct engine_s *engine);
void intro_update(struct scene_s *scene, struct engine_s *engine, float dt);
void intro_draw(struct scene_s *scene, struct engine_s *engine);

#endif

