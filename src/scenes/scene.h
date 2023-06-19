#ifndef SCENE_H
#define SCENE_H

struct engine_s;

struct scene_s {
	void (*load)(struct scene_s *scene, struct engine_s *engine);
	void (*destroy)(struct scene_s *scene, struct engine_s *engine);
	void (*update)(struct scene_s *scene, struct engine_s *engine, float dt);
	void (*draw)(struct scene_s *scene, struct engine_s *engine);
};

void scene_init(struct scene_s *scene, struct engine_s *engine);
void scene_destroy(struct scene_s *scene, struct engine_s *engine);
void scene_load(struct scene_s *scene, struct engine_s *engine);
void scene_update(struct scene_s *scene, struct engine_s *engine, float dt);
void scene_draw(struct scene_s *scene, struct engine_s *engine);

#endif

