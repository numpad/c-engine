#ifndef ENGINE_H
#define ENGINE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

typedef struct {
	SDL_Window *window;
	Uint32 window_id;
	SDL_GLContext gl_ctx;
	int is_running;
	float mouseX, mouseY;
} engine_t;

int engine_init(engine_t *engine);
int engine_destroy(engine_t *engine);

void engine_update(engine_t *engine);
void engine_draw(engine_t *engine);
void engine_mainloop(engine_t *engine);
void engine_mainloop_emcc(void *engine);

#endif

