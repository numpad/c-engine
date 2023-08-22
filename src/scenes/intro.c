#include "intro.h"
#include <stdlib.h>
#include <math.h>
#include <SDL.h>
#include "scene.h"
#include "engine.h"
#include "menu.h"
#include "gl/shader.h"
#include "gl/vbuffer.h"
#include "gl/texture.h"

static void switch_to_menu_scene(struct engine_s *engine) {
	struct menu_s *menu = malloc(sizeof(struct menu_s));
	menu_init(menu, engine);
	engine_setscene(engine, (struct scene_s *)menu);
}

static void intro_load(struct intro_s *scene, struct engine_s *engine) {
	nvgCreateFont(engine->vg, "PermanentMarker Regular", "res/font/PermanentMarker-Regular.ttf");

	scene->timer = 0.0f;
	scene->time_passed = 0.0f;
	scene->time_passed_max = 2.0f;

	scene->logo_image_nvg = nvgCreateImage(engine->vg, "res/image/numpad.png", NVG_IMAGE_NEAREST);
}

static void intro_destroy(struct intro_s *scene, struct engine_s *engine) {
	nvgDeleteImage(engine->vg, scene->logo_image_nvg);
}

static void intro_update(struct intro_s *scene, struct engine_s *engine, float dt) {
	scene->time_passed += dt;

	if (scene->time_passed >= 3.5f) {
		switch_to_menu_scene(engine);
	}
}

static inline float ease_out_expo(float n) {
	return 1.0 - pow(2.0, -10.0 * n);
}

static void intro_draw(struct intro_s *scene, struct engine_s *engine) {
	glClearColor(0.24f, 0.58f, 1.0f, 1.0f);
	int mx, my;
	const Uint32 buttons = SDL_GetMouseState(&mx, &my);
	if (buttons & SDL_BUTTON(1)) {
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

	const float xcenter = engine->window_width * 0.5f;
	const float ycenter = engine->window_height * 0.5f;
	const float halfsize = fminf(engine->window_width, engine->window_height) * 0.2f;
	const float img_alpha = glm_clamp(glm_ease_cubic_in(scene->time_passed / scene->time_passed_max) * 4.0f - 0.2f, 0.0f, 1.0f);

	NVGcontext *vg = engine->vg;
	nvgBeginPath(vg);
	nvgRect(vg, xcenter - halfsize, ycenter - halfsize, halfsize * 2.0f, halfsize * 2.0f);
	NVGpaint paint = nvgImagePattern(vg, xcenter - halfsize, ycenter - halfsize, halfsize * 2.0f, halfsize * 2.0f, 0.0f, scene->logo_image_nvg, img_alpha);
	nvgFillPaint(vg, paint);
	nvgFill(vg);

}

void intro_init(struct intro_s *intro, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)intro, engine);

	// init function pointers
	intro->base.load = (scene_load_fn)intro_load;
	intro->base.destroy = (scene_destroy_fn)intro_destroy;
	intro->base.update = (scene_update_fn)intro_update;
	intro->base.draw = (scene_draw_fn)intro_draw;

}

