#ifndef MENU_H
#define MENU_H

#include "scene.h"

struct engine_s;
struct isoterrain_s;

struct menu_s {
	struct scene_s base;
	struct isoterrain_s *terrain;
};

void scene_menu_init(struct menu_s *menu, struct engine_s *engine);

#endif

