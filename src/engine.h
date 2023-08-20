#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <SDL_opengles2.h>
#include <cglm/cglm.h>
#include "scenes/scene.h"
#include <nanovg.h>

struct engine_s;
struct console_s;
typedef void(*engine_callback_fn)(struct engine_s*);

struct engine_s {
	// windowing
	SDL_Window *window;
	Uint32 window_id;
	SDL_GLContext gl_ctx;
	struct NVGcontext *vg;
	
	int window_width, window_height;
	float window_aspect;

	// scene management
	struct scene_s *scene;

	// hooks
	engine_callback_fn *on_notify_callbacks;

	// others
	struct console_s *console;

	// rendering globals
	mat4 u_projection;
	mat4 u_view;
};

// init & cleanup
struct engine_s *engine_new();
int engine_destroy(struct engine_s *engine);

// scene handling
void engine_setscene(struct engine_s *engine, struct scene_s *scene);
void engine_setscene_dll(struct engine_s *engine, const char *filename);

// main loop
void engine_update(struct engine_s *engine);
void engine_draw(struct engine_s *engine);
void engine_mainloop(struct engine_s *engine);
void engine_mainloop_emcc(void *engine);

#endif

