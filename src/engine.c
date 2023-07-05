#include "engine.h"
#include <SDL.h>
#define NANOVG_GLES2_IMPLEMENTATION
#include <nanovg_gl.h>
#undef NANOVG_GLES2_IMPLEMENTATION
#include <SDL_net.h>
#include "scenes/intro.h"

struct engine_s *engine_new() {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS)) {
		SDL_Log("failed initializing SDL: %s\n", SDL_GetError());
		return NULL;
	}

	struct engine_s *engine = malloc(sizeof(struct engine_s));
	engine->window = SDL_CreateWindow("Soil Soldiers", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 450, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
	engine->window_id = SDL_GetWindowID(engine->window);

	if (engine->window == NULL) {
		SDL_Log("failed initializing window\n");
		return NULL;
	}

	if (SDLNet_Init() < 0) {
		SDL_Log("failed initializing SDL_net\n");
		return NULL;
	}


	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetSwapInterval(1);
	engine->gl_ctx = SDL_GL_CreateContext(engine->window);

	engine->vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

	// scene
	struct intro_s *intro = malloc(sizeof(struct intro_s));
	intro_init(intro, engine);
	engine->scene = NULL;
	engine_setscene(engine, (struct scene_s *)intro);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glViewport(0, 0, 450, 800);
	engine->window_width = 450;
	engine->window_height = 800;

	return engine;
}

int engine_destroy(struct engine_s *engine) {
	// scene

	// other
	nvgDeleteGLES2(engine->vg);

	// windowing
	SDL_DestroyWindow(engine->window);
	SDL_GL_DeleteContext(engine->gl_ctx);

	free(engine);
	return 0;
}

//
// scene handling
//

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

#ifdef DEBUG
#include <dlfcn.h>
void engine_setscene_dll(struct engine_s *engine, const char *filename) {
	static void *handle = NULL;
	if (handle != NULL) {
		dlclose(handle);
	}

	handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);

	struct scene_s *modscene = malloc(sizeof(struct scene_s));
	void(*test_init)(struct scene_s *scene, struct engine_s *) = dlsym(handle, "test_init");
	test_init(modscene, engine);

	scene_load(modscene, engine);
	scene_update(modscene, engine, 1.0f / 60.0f);
	scene_draw(modscene, engine);
	scene_destroy(modscene, engine);

	engine->scene = modscene;

	// TODO: dlclose(handle);
}
#else
void engine_setscene_dll(struct engine_s *engine, const char *filename) {
	fprintf(stderr, "dll loading disabled for non-debug builds!\n");
}
#endif

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
					engine->window_width = event.window.data1;
					engine->window_height = event.window.data2;
					glViewport(0, 0, event.window.data1, event.window.data2);
				}
				break;
			case SDL_KEYDOWN: {
				SDL_KeyboardEvent *key_event = (SDL_KeyboardEvent *)&event;
				if (key_event->keysym.sym == SDLK_BACKSPACE) {
					engine_setscene_dll(engine, "./libtest.so");
				}
				break;
			}
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
	
	nvgBeginFrame(engine->vg, engine->window_width, engine->window_height, 1.0f);

	// run scene
	scene_update(engine->scene, engine, 1.0f / 60.0f);
	scene_draw(engine->scene, engine);

	nvgEndFrame(engine->vg);

	SDL_GL_SwapWindow(engine->window);
}

void engine_mainloop(struct engine_s *engine) {
	engine_update(engine);
	engine_draw(engine);
}

void engine_mainloop_emcc(void *engine) {
	engine_mainloop((struct engine_s *)engine);
}

