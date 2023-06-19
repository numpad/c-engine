#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <SDL_opengles2.h>
#include "game.h"
#include "scenes/scene.h"

struct engine_s {
	// windowing
	SDL_Window *window;
	SDL_Renderer *renderer;
	Uint32 window_id;
	SDL_GLContext gl_ctx;
	int is_running;
	
	game_t *game;
	struct scene_s *scene;
	float mouseX, mouseY;
};

struct engine_s *engine_new();
int engine_destroy(struct engine_s *engine);

void engine_update(struct engine_s *engine);
void engine_draw(struct engine_s *engine);
void engine_mainloop(struct engine_s *engine);
void engine_mainloop_emcc(void *engine);

#endif

