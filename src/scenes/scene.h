#ifndef SCENE_H
#define SCENE_H

//
// base class for a scene.
// TODO: put `engine_s *` into struct on init so `scene_*_fn` don't need it as param.
//

struct engine_s;
struct scene_s;

typedef void(*scene_load_fn)(struct scene_s *, struct engine_s *);
typedef void(*scene_destroy_fn)(struct scene_s *, struct engine_s *);
typedef void(*scene_update_fn)(struct scene_s *, struct engine_s *, float);
typedef void(*scene_draw_fn)(struct scene_s *, struct engine_s *);

struct scene_s {
	scene_load_fn load;
	scene_destroy_fn destroy;
	scene_update_fn update;
	scene_draw_fn draw;
};

void scene_init(struct scene_s *scene, struct engine_s *engine);
void scene_destroy(struct scene_s *scene, struct engine_s *engine);
void scene_load(struct scene_s *scene, struct engine_s *engine);
void scene_update(struct scene_s *scene, struct engine_s *engine, float dt);
void scene_draw(struct scene_s *scene, struct engine_s *engine);

#endif

