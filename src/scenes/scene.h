#ifndef SCENE_H
#define SCENE_H

//
// base class for a scene.
// TODO: put `engine_s *` into struct on init so `scene_*_fn` don't need it as param.
//

#include "event.h"

struct engine;
struct scene_s;
struct message_header;

typedef void(*scene_load_fn)(struct scene_s *, struct engine *);
typedef void(*scene_destroy_fn)(struct scene_s *, struct engine *);
typedef void(*scene_update_fn)(struct scene_s *, struct engine *, float);
typedef void(*scene_draw_fn)(struct scene_s *, struct engine *);
typedef void(*scene_on_message_fn)(struct scene_s *, struct engine *, struct message_header *);
typedef void(*scene_on_callback_fn)(struct scene_s *, struct engine *, struct engine_event);

struct scene_s {
	scene_load_fn load;
	scene_destroy_fn destroy;
	scene_update_fn update;
	scene_draw_fn draw;
	scene_on_message_fn on_message;
	scene_on_callback_fn on_callback;
};

void scene_init(struct scene_s *scene, struct engine *engine);
void scene_destroy(struct scene_s *scene, struct engine *engine);
void scene_load(struct scene_s *scene, struct engine *engine);
void scene_update(struct scene_s *scene, struct engine *engine, float dt);
void scene_draw(struct scene_s *scene, struct engine *engine);
void scene_on_message(struct scene_s *scene, struct engine *engine, struct message_header *);
void scene_on_callback(struct scene_s *scene, struct engine *engine, struct engine_event);

#endif

