#include "menu.h"
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include <nanovg_gl.h>
#include "engine.h"

static void menu_load(struct menu_s *menu, struct engine_s *engine) {
}

static void menu_destroy(struct menu_s *menu, struct engine_s *engine) {
}

static void menu_update(struct menu_s *menu, struct engine_s *engine, float dt) {
}

static void menu_draw(struct menu_s *menu, struct engine_s *engine) {
	int width, height;
	SDL_GetWindowSize(engine->window, &width, &height);

	//rectangleColor(engine->renderer, );
	stringColor(engine->renderer, 100, 150, "menu scene", 0xff999999);
	stringColor(engine->renderer, 102, 152, "menu scene", 0xffbbbbbb);
	stringColor(engine->renderer, 104, 154, "menu scene", 0xffffffff);

	const margin = 48;
	roundedBoxColor(engine->renderer, 0 + margin, 250, width - margin, 250 + 30, 8, 0xff7eafdd);
	roundedRectangleColor(engine->renderer, 0 + margin, 250, width - margin, 250 + 30, 8, 0xff7e80dd);

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

