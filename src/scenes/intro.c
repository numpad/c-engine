#include "intro.h"
#include <stdlib.h>
#include <math.h>
#include <SDL.h>
#include "scene.h"
#include "engine.h"
#include "menu.h"

static void switch_to_menu_scene(struct engine_s *engine) {
	struct menu_s *menu = malloc(sizeof(struct menu_s));
	menu_init(menu, engine);
	engine_setscene(engine, (struct scene_s *)menu);
}

static void intro_load(struct intro_s *scene, struct engine_s *engine) {
	nvgCreateFont(engine->vg, "PermanentMarker Regular", "res/font/PermanentMarker-Regular.ttf");

	scene->time_passed = 0.0f;
}

static void intro_destroy(struct intro_s *scene, struct engine_s *engine) {
}

static void intro_update(struct intro_s *scene, struct engine_s *engine, float dt) {
	scene->time_passed += dt;

	if (scene->time_passed >= 1.5f) {
		switch_to_menu_scene(engine);
	}
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
			switch_to_menu_scene(engine);
		}
	} else {
		scene->timer *= 0.65f;
	}

	float fill = scene->timer;
	if (fill > 1.0) fill = 1.0f;
	fill = ease_out_expo(fill);

	// boxColor(engine->renderer, mx - 50 * fill, my - 50 * fill, mx + 50 * fill, my + 50 * fill, 0xFF0000FF);
	// rectangleColor(engine->renderer, mx - 50, my - 50, mx + 50, my + 50, 0xFFFF0000);
	// stringColor(engine->renderer, mx, my + 50, "intro scene", 0xffffffff);
	nvgBeginPath(engine->vg);
	nvgCircle(engine->vg, mx, my, 30.0f * fill);
	nvgFillColor(engine->vg, nvgRGBA(200, 170, 190, 128));
	nvgFill(engine->vg);

	nvgBeginPath(engine->vg);
	nvgCircle(engine->vg, mx, my, 20.0f + 10.0f * fill);
	nvgStrokeWidth(engine->vg, 1.0f + 1.0f * fill);
	nvgStrokeColor(engine->vg, nvgRGBA(150, 110, 130, 200));
	nvgStroke(engine->vg);

#ifdef DEBUG
	nvgBeginPath(engine->vg);
	nvgFillColor(engine->vg, nvgRGB(100, 220, 100));
	nvgFontSize(engine->vg, 28.0f);
	nvgText(engine->vg, 100.0f, 200.0f, "DEBUG = true", NULL);
#else
	nvgBeginPath(engine->vg);
	nvgFillColor(engine->vg, nvgRGB(220, 100, 100));
	nvgFontSize(engine->vg, 28.0f);
	nvgText(engine->vg, 100.0f, 200.0f, "DEBUG = false", NULL);
#endif

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

