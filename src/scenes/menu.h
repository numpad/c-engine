#ifndef MENU_H
#define MENU_H

#include "scene.h"

struct engine_s;

struct menu_s {
	struct scene_s base;
};

void scene_menu_init(struct menu_s *menu, struct engine_s *engine);

#endif

