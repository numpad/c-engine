#include "experiments.h"

#include <stdlib.h>
#include <assert.h>
#include <nanovg.h>
#include <nuklear.h>
#include <cglm/cglm.h>
#include "engine.h"

//
// scene functions
//
static void load(struct scene_experiments_s *scene, struct engine_s *engine) {
	engine_set_clear_color(0.99, 0.92, 0.17);
}

static void destroy(struct scene_experiments_s *scene, struct engine_s *engine) {
}

static void update(struct scene_experiments_s *scene, struct engine_s *engine, float dt) {
}

static void draw(struct scene_experiments_s *scene, struct engine_s *engine) {
}

void scene_experiments_init(struct scene_experiments_s *scene, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)scene, engine);
	
	// init function pointers
	scene->base.load = (scene_load_fn)load;
	scene->base.destroy = (scene_destroy_fn)destroy;
	scene->base.update = (scene_update_fn)update;
	scene->base.draw = (scene_draw_fn)draw;
}

