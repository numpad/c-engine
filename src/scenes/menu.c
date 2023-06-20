#include "menu.h"
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>

static void menu_load(struct menu_s *menu, struct engine_s *engine) {
}

static void menu_destroy(struct menu_s *menu, struct engine_s *engine) {
}

static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
}

void menu_init(struct menu_s *menu, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)menu, engine);

	// init function pointers
	menu->base.load = (scene_load_fn)menu_load;
	menu->base.destroy = (scene_destroy_fn)menu_destroy;
	menu->base.update = (scene_update_fn)menu_update;
	menu->base.draw = (scene_draw_fn)menu_draw;

}

