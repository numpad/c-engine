#ifndef MENU_H
#define MENU_H

#include "scene.h"

struct engine_s;
struct ugui_s;

struct menu_s {
	struct scene_s base;

	struct ugui_s *gui;
	int vg_font;
};

void mod_func(struct menu_s *menu, struct engine_s *engine);
void menu_init(struct menu_s *menu, struct engine_s *engine);

#endif

