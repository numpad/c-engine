#include <SDL.h>
#include "scenes/scene.h"
#include <nanovg.h>
#include "engine.h"

struct test_s {
	struct scene_s base;

};

static void test_load(struct test_s *scene, struct engine_s *engine) {
	SDL_Log("test_load");
}

static void test_destroy(struct test_s *scene, struct engine_s *engine) {
	SDL_Log("test_destroy");
}

static void test_update(struct test_s *scene, struct engine_s *engine, float dt) {
	SDL_Log("test_update %g", dt);
}

static void test_draw(struct test_s *scene, struct engine_s *engine) {
	SDL_Log("test_draw");
	static float pos = 0.0f;
	static float vel = 0.0f;
	vel += 0.5f;
	pos += vel;

	nvgBeginPath(engine->vg);
	nvgCircle(engine->vg, 30.0f, pos, 50.0f);
	nvgFillColor(engine->vg, nvgRGBAf(0.5f, 0.1f, 0.8f, 0.75f));
	nvgFill(engine->vg);
}

void test_init(struct test_s *test, struct engine_s *engine) {
	SDL_Log("loadin' stuff");
	// init scene base
	scene_init((struct scene_s *)test, engine);

	// init function pointers
	test->base.load = (scene_load_fn)test_load;
	test->base.destroy = (scene_destroy_fn)test_destroy;
	test->base.update = (scene_update_fn)test_update;
	test->base.draw = (scene_draw_fn)test_draw;

}

