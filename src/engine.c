#include "engine.h"
#include <SDL.h>
#include <SDL2_gfxPrimitives.h>
#include "scenes/intro.h"

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
	struct intro_s *intro = malloc(sizeof(struct intro_s));
	intro_init(intro, engine);
	engine->scene = NULL;
	engine_setscene(engine, (struct scene_s *)intro);

	glClearColor(183.0f / 255.0f, 255.0f / 255.0f, 210.0f / 255.0f, 1.0f);
	glViewport(0, 0, 450, 800);

	return engine;
}

int engine_destroy(struct engine_s *engine) {
	// scene
	// windowing
	SDL_DestroyWindow(engine->window);
	SDL_DestroyRenderer(engine->renderer);
	SDL_GL_DeleteContext(engine->gl_ctx);

	free(engine);
	return 0;
}

// scene handling
void engine_setscene(struct engine_s *engine, struct scene_s *new_scene) {
	struct scene_s *old_scene = engine->scene;
	engine->scene = NULL;

	// cleanup previous scene
	if (old_scene != NULL) {
		scene_destroy(old_scene, engine);
		free(old_scene);
	}
	
	if (new_scene != NULL) {
		engine->scene = new_scene;
		scene_load(new_scene, engine);
	}
}

// main loop
void engine_update(struct engine_s *engine) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				engine_setscene(engine, NULL);
				break;
			case SDL_WINDOWEVENT:
				if (/* event.window.windowID == engine->window_id && */ event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					glViewport(0, 0, event.window.data1, event.window.data2);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			{
				//SDL_MouseButtonEvent *mouse_event = (SDL_MouseButtonEvent *)&event;
				break;
			}
		}
	}
}

void engine_draw(struct engine_s *engine) {
	glClear(GL_COLOR_BUFFER_BIT);
	
	int w, h;
	SDL_GetWindowSize(engine->window, &w, &h);
	char text[128];
	sprintf(text, "window: %dx%d", w, h);
	stringColor(engine->renderer, 10, 10, text, 0xFF00FFFF);

	SDL_version version;
	SDL_GetVersion(&version);
	sprintf(text, "version: %u.%u.%u", version.major, version.minor, version.patch);
	stringColor(engine->renderer, 10, 20, text, 0xFF00FFFF);

	// run scene
	scene_update(engine->scene, engine, 1.0f / 60.0f);
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

