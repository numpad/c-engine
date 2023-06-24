#include "intro.h"
#include <stdlib.h>
#include <math.h>
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include "scene.h"
#include "engine.h"
#include "menu.h"

static void intro_load(struct intro_s *scene, struct engine_s *engine) {
}

static void intro_destroy(struct intro_s *scene, struct engine_s *engine) {
}

static void intro_update(struct intro_s *scene, struct engine_s *engine, float dt) {
}

static inline float ease_out_expo(float n) {
	return 1.0 - pow(2.0, -10.0 * n);
}

static void intro_draw(struct intro_s *scene, struct engine_s *engine) {
	int mx, my;
	const Uint32 buttons = SDL_GetMouseState(&mx, &my);
	if (SDL_BUTTON(buttons) & 1) {
		scene->timer += 0.05f;
		if (scene->timer >= 1.0f) {
			struct menu_s *menu = malloc(sizeof(struct menu_s));
			menu_init(menu, engine);
			engine_setscene(engine, (struct scene_s *)menu);
		}
	} else {
		scene->timer = 0.0f;
	}

	float fill = scene->timer;
	if (fill > 1.0) fill = 1.0f;
	fill = ease_out_expo(fill);

	boxColor(engine->renderer, mx - 50 * fill, my - 50 * fill, mx + 50 * fill, my + 50 * fill, 0xFF0000FF);
	rectangleColor(engine->renderer, mx - 50, my - 50, mx + 50, my + 50, 0xFFFF0000);
	stringColor(engine->renderer, mx, my + 50, "intro scene", 0xffffffff);
}

void intro_init(struct intro_s *intro, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)intro, engine);

	// init function pointers
	intro->base.load = (scene_load_fn)intro_load;
	intro->base.destroy = (scene_destroy_fn)intro_destroy;
	intro->base.update = (scene_update_fn)intro_update;
	intro->base.draw = (scene_draw_fn)intro_draw;

	intro->timer = 0.0f;
}

