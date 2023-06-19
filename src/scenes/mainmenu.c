#include "mainmenu.h"

#include <SDL2_gfxPrimitives.h>
#include "engine.h"

struct mainmenu_s *mainmenu_new() {
	struct mainmenu_s *menu = malloc(sizeof(struct mainmenu_s));
	return menu;
}

int mainmenu_destroy(struct mainmenu_s *menu) {
	free(menu);
	return 0;
}

void mainmenu_draw(struct mainmenu_s *menu, struct engine_s *engine) {
	rectangleColor(engine->renderer, 100, 100, 200, 130, 0xff0000ff);
}

