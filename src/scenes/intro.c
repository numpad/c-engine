#include "intro.h"
#include <stdlib.h>
#include <math.h>
#include <SDL.h>
#include <stb_perlin.h>
#include "scene.h"
#include "engine.h"
#include "menu.h"
#include "gui/console.h"

static void switch_to_menu_scene(struct engine_s *engine) {
	struct scene_menu_s *menu = malloc(sizeof(struct scene_menu_s));
	scene_menu_init(menu, engine);
	engine_setscene(engine, (struct scene_s *)menu);
}

static void intro_load(struct scene_intro_s *scene, struct engine_s *engine) {
	scene->timer = 0.0f;
	scene->time_passed = 0.0f;
	scene->time_passed_max = 1.75f;

	scene->logo_image_nvg = nvgCreateImage(engine->vg, "res/image/numpad.png", NVG_IMAGE_NEAREST);
}

static void intro_destroy(struct scene_intro_s *scene, struct engine_s *engine) {
	nvgDeleteImage(engine->vg, scene->logo_image_nvg);
}

static void intro_update(struct scene_intro_s *scene, struct engine_s *engine, float dt) {
	scene->time_passed += dt;

	if (scene->time_passed >= scene->time_passed_max) {
		switch_to_menu_scene(engine);
	}
}

static inline float ease_out_expo(float n) {
	return 1.0 - pow(2.0, -10.0 * n);
}

static void intro_draw(struct scene_intro_s *scene, struct engine_s *engine) {
	engine_set_clear_color(0.24f, 0.58f, 1.0f);
	int mx, my;
	const Uint32 buttons = SDL_GetMouseState(&mx, &my);
	if (buttons & SDL_BUTTON(1)) {
		scene->timer += engine->dt;
		if (scene->timer >= 1.0f) {
			switch_to_menu_scene(engine);
		}
	} else {
		scene->timer *= 0.65f;
	}

	float fill = scene->timer;
	if (fill > 1.0) fill = 1.0f;
	fill = ease_out_expo(fill);

	// draw logo
	const float noisex = stb_perlin_noise3(scene->timer * scene->timer * 15.0f, scene->timer * 3.0f, 0.0f, 20.0f, 20.0f, 20.0f) * 12.0f;
	const float noisey = stb_perlin_noise3(scene->timer * 3.0f, scene->timer * scene->timer * 15.0f, 0.0f, 20.0f, 20.0f, 20.0f) * 12.0f;

	const float xcenter = engine->window_width * 0.5f + noisex;
	const float ycenter = engine->window_height * 0.5f + noisey;
	const float halfsize = fminf(engine->window_width, engine->window_height) * 0.33f * (1.0f - glm_ease_quad_out(scene->timer) * 0.25f);
	const float x = glm_ease_quad_in(scene->time_passed / scene->time_passed_max);
	const float img_alpha = glm_clamp(-fabsf((x - 0.5f) * 2.9f)+1.4f, 0.0f, 1.0f);
	NVGcontext *vg = engine->vg;
	nvgBeginPath(vg);
	nvgRect(vg, xcenter - halfsize, ycenter - halfsize, halfsize * 2.0f, halfsize * 2.0f);
	NVGpaint paint = nvgImagePattern(vg, xcenter - halfsize, ycenter - halfsize, halfsize * 2.0f, halfsize * 2.0f, 0.0f, scene->logo_image_nvg, img_alpha);
	nvgFillPaint(vg, paint);
	nvgFill(vg);

}

void scene_intro_init(struct scene_intro_s *intro, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)intro, engine);

	// init function pointers
	intro->base.load = (scene_load_fn)intro_load;
	intro->base.destroy = (scene_destroy_fn)intro_destroy;
	intro->base.update = (scene_update_fn)intro_update;
	intro->base.draw = (scene_draw_fn)intro_draw;

}

