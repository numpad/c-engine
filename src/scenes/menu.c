#include "menu.h"
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include "engine.h"

static void menu_load(struct menu_s *menu, struct engine_s *engine) {
}

static void menu_destroy(struct menu_s *menu, struct engine_s *engine) {
}

static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
	stringColor(engine->renderer, 100, 150, "menu scene", 0xff999999);
	stringColor(engine->renderer, 102, 152, "menu scene", 0xffbbbbbb);
	stringColor(engine->renderer, 104, 154, "menu scene", 0xffffffff);
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

