#include "intro.h"
#include <stdlib.h>
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include "scene.h"
#include "engine.h"

struct intro_s *intro_new(struct engine_s *engine) {
	struct intro_s *intro = malloc(sizeof(struct intro_s));

	scene_init((struct scene_s*)intro, engine);
	intro->base.load = intro_load;
	intro->base.destroy = intro_destroy;
	intro->base.update = intro_update;
	intro->base.draw = intro_draw;

	return intro;
}

void intro_destroy(struct scene_s *scene, struct engine_s *engine) {
}

void intro_load(struct scene_s *scene, struct engine_s *engine) {
}

void intro_update(struct scene_s *scene, struct engine_s *engine, float dt) {
}

void intro_draw(struct scene_s *scene, struct engine_s *engine) {
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	printf("mouse: %d, %d\n", mx, my);

	rectangleColor(engine->renderer, mx - 50, my - 50, mx + 50, my + 50, 0xFF0000FF);
	thickLineColor(engine->renderer, 10, 10, engine->mouseX, engine->mouseY, 3, 0xffff00ff);
	stringColor(engine->renderer, 20, 80, "hello: world", 0xffffffff);
}

