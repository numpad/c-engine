#include "engine.h"
#include <SDL2_gfxPrimitives.h>
#include "scenes/intro.h"

void introscene_draw(struct engine_s *engine) {

	thickLineColor(engine->renderer, 10, 10, engine->mouseX, engine->mouseY, 3, 0xffff00ff);
	stringColor(engine->renderer, 20, 80, "hello: world", 0xffffffff);
}

struct engine_s *engine_new() {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS)) {
		SDL_Log("failed initializing SDL: %s\n", SDL_GetError());
		return NULL;
	}

	struct engine_s *engine = malloc(sizeof(struct engine_s));
	engine->window = SDL_CreateWindow("Soil Soldiers", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 450, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
	engine->window_id = SDL_GetWindowID(engine->window);
	engine->renderer = SDL_CreateRenderer(engine->window, -1, SDL_RENDERER_ACCELERATED);

	if (engine->renderer == NULL) {
		SDL_Log("failed initializing renderer\n");
		return NULL;
	}


	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetSwapInterval(1);
	engine->gl_ctx = SDL_GL_CreateContext(engine->window);

	// scene
	struct intro_s *intro = intro_new(engine);
	engine->scene = (struct scene_s *)intro;

	engine->is_running = 1;

	// init game
	engine->game = malloc(sizeof(*engine->game));
	game_init(engine->game, 16, 16);

	glClearColor(183.0f / 255.0f, 255.0f / 255.0f, 210.0f / 255.0f, 1.0f);
	glViewport(0, 0, 450, 800);

	return engine;
}

int engine_destroy(struct engine_s *engine) {
	// game
	game_destroy(engine->game);
	free(engine->game);
	// windowing
	SDL_DestroyWindow(engine->window);
	SDL_DestroyRenderer(engine->renderer);
	SDL_GL_DeleteContext(engine->gl_ctx);

	free(engine);
	return 0;
}

void engine_update(struct engine_s *engine) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				engine->is_running = 0;
				break;
			case SDL_WINDOWEVENT:
				if (event.window.windowID == engine->window_id && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					glViewport(0, 0, event.window.data1, event.window.data2);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			{
				SDL_MouseButtonEvent *mouse_event = (SDL_MouseButtonEvent *)&event;
				engine->mouseX = mouse_event->x;
				engine->mouseY = mouse_event->y;
				glClearColor(engine->mouseX / 450.0f, engine->mouseY / 800.0f, 0.0f, 1.0f);
				break;
			}
		}
	}
}

void engine_draw(struct engine_s *engine) {
	glClear(GL_COLOR_BUFFER_BIT);
	
	// run scene
	//introscene_draw(engine);
	scene_draw(engine->scene, engine);


	SDL_RenderPresent(engine->renderer);
	//SDL_GL_SwapWindow(engine->window);
}

void engine_mainloop(struct engine_s *engine) {
	engine_update(engine);
	engine_draw(engine);
}

void engine_mainloop_emcc(void *engine) {
	engine_mainloop((struct engine_s *)engine);
}

