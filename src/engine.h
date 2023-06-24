#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <SDL_opengles2.h>
#include "scenes/scene.h"

struct engine_s {
	// windowing
	SDL_Window *window;
	SDL_Renderer *renderer;
	Uint32 window_id;
	SDL_GLContext gl_ctx;
	
	struct scene_s *scene;
};

// init & cleanup
struct engine_s *engine_new();
int engine_destroy(struct engine_s *engine);

// scene handling
void engine_setscene(struct engine_s *engine, struct scene_s *scene);

// main loop
void engine_update(struct engine_s *engine);
void engine_draw(struct engine_s *engine);
void engine_mainloop(struct engine_s *engine);
void engine_mainloop_emcc(void *engine);

#endif

