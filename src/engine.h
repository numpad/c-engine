#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <SDL_opengles2.h>
#include "scenes/scene.h"
#include <nanovg.h>

struct engine_s {
	// windowing
	SDL_Window *window;
	Uint32 window_id;
	SDL_GLContext gl_ctx;
	
	int window_width, window_height;

	struct scene_s *scene;
	struct NVGcontext *vg;
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

