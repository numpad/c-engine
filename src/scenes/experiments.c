#include "experiments.h"

static void scene_experiments_load(struct scene_experiments_s *scene, struct engine_s *engine) {
}

static void scene_experiments_destroy(struct scene_experiments_s *scene, struct engine_s *engine) {
}

static void scene_experiments_update(struct scene_experiments_s *scene, struct engine_s *engine, float dt) {
}

static void scene_experiments_draw(struct scene_experiments_s *scene, struct engine_s *engine) {
}

void scene_experiments_init(struct scene_experiments_s *scene, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)scene, engine);
	
	// init function pointers
	scene->base.load = (scene_load_fn)scene_experiments_load;
	scene->base.destroy = (scene_destroy_fn)scene_experiments_destroy;
	scene->base.update = (scene_update_fn)scene_experiments_update;
	scene->base.draw = (scene_draw_fn)scene_experiments_draw;
}

