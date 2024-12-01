#include "scene.h"
#include <stdlib.h>

void scene_init(struct scene_s *scene, struct engine *engine) {
	scene->load = NULL;
	scene->destroy = NULL;
	scene->update = NULL;
	scene->draw = NULL;
	scene->on_message = NULL;
	scene->on_callback = NULL;
}

void scene_destroy(struct scene_s *scene, struct engine *engine) {
	if (scene != NULL && scene->destroy != NULL) {
		scene->destroy(scene, engine);
	}
}

void scene_load(struct scene_s *scene, struct engine *engine) {
	if (scene != NULL && scene->load != NULL) {
		scene->load(scene, engine);
	}
}

void scene_update(struct scene_s *scene, struct engine *engine, float dt) {
	if (scene != NULL && scene->update != NULL) {
		scene->update(scene, engine, dt);
	}
}

void scene_draw(struct scene_s *scene, struct engine *engine) {
	if (scene != NULL && scene->draw != NULL) {
		scene->draw(scene, engine);
	}
}

void scene_on_message(struct scene_s *scene, struct engine *engine, struct message_header *msg) {
	if (scene != NULL && scene->on_message != NULL) {
		scene->on_message(scene, engine, msg);
	}
}

void scene_on_callback(struct scene_s *scene, struct engine *engine, struct engine_event event) {
	if (scene != NULL && scene->on_callback != NULL) {
		scene->on_callback(scene, engine, event);
	}
}
