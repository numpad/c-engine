#ifndef MENU_H
#define MENU_H

#include "scene.h"

struct engine_s;
struct ugui_s;
struct isoterrain_s;

struct menu_s {
	struct scene_s base;

	struct ugui_s *gui;
	int vg_font;
	int vg_gamelogo;

	struct isoterrain_s *terrain;
};

void menu_init(struct menu_s *menu, struct engine_s *engine);

#endif

