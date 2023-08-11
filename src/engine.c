#include "engine.h"
#include <SDL.h>
#define NANOVG_GLES2_IMPLEMENTATION
#include <nanovg_gl.h>
#undef NANOVG_GLES2_IMPLEMENTATION
#include <SDL_net.h>
#include <stb_ds.h>
#include "scenes/intro.h"
#include "scenes/battlebadgers.h"

static Uint32 USR_EVENT_RELOAD = ((Uint32)-1);
static Uint32 USR_EVENT_NOTIFY = ((Uint32)-1);

static void window_resize(struct engine_s *engine, int w, int h) {
	engine->window_width = w;
	engine->window_height = h;
	glViewport(0, 0, w, h);
	const float aspect = h / (float)w;
	glm_ortho(-1.0f, 1.0f, -1.0f * aspect, 1.0f * aspect, -1.0f, 1.0f, engine->u_projection);
}

#ifdef __unix__
#include <signal.h>
void on_sigusr1(int signum) {
	if (USR_EVENT_RELOAD != ((Uint32)-1)) {
		SDL_Event event;
		SDL_memset(&event, 0, sizeof(event)); /* or SDL_zero(event) */
		event.type = USR_EVENT_RELOAD;
		event.user.code = 0;
		event.user.data1 = NULL;
		event.user.data2 = NULL;
		SDL_PushEvent(&event);
	}
	signal(SIGUSR1, on_sigusr1);
}
void on_sigusr2(int signum) {
	if (USR_EVENT_NOTIFY != ((Uint32)-1)) {
		SDL_Event event;
		SDL_memset(&event, 0, sizeof(event)); /* or SDL_zero(event) */
		event.type = USR_EVENT_NOTIFY;
		event.user.code = 0;
		event.user.data1 = NULL;
		event.user.data2 = NULL;
		SDL_PushEvent(&event);
	}
	signal(SIGUSR2, on_sigusr2);
}
#endif

struct engine_s *engine_new(void) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL: %s.\n", SDL_GetError());
		return NULL;
	}

	struct engine_s *engine = malloc(sizeof(struct engine_s));
	engine->window_width = 550;
	engine->window_height = 800;
	engine->window = SDL_CreateWindow("Soil Soldiers", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, engine->window_width, engine->window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
	engine->window_id = SDL_GetWindowID(engine->window);
	engine->on_notify_callbacks = NULL;

	if (engine->window == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL window.\n");
		return NULL;
	}

	if (SDLNet_Init() < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL_net.\n");
		return NULL;
	}

	// OpenGL config
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetSwapInterval(1);
	engine->gl_ctx = SDL_GL_CreateContext(engine->window);

	
	// libs
	engine->vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

	// custom events
	USR_EVENT_RELOAD = SDL_RegisterEvents(1);
	USR_EVENT_NOTIFY = SDL_RegisterEvents(2);
#ifdef __unix__
	signal(SIGUSR1, on_sigusr1);
	signal(SIGUSR2, on_sigusr2);
#endif

	// scene
	struct intro_s *intro = malloc(sizeof(struct intro_s));
	intro_init(intro, engine);
	engine->scene = NULL;
	engine_setscene(engine, (struct scene_s *)intro);

	window_resize(engine, engine->window_width, engine->window_height);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// camera
	glm_mat4_identity(engine->u_view);

	return engine;
}

int engine_destroy(struct engine_s *engine) {
	// scene
	engine_setscene(engine, NULL);

	// other
	nvgDeleteGLES2(engine->vg);

	// windowing
	SDL_DestroyWindow(engine->window);
	SDL_GL_DeleteContext(engine->gl_ctx);

	SDLNet_Quit();
	SDL_Quit();

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
	engine_setscene(engine, NULL);

	static void *handle = NULL;
	if (handle != NULL) {
		dlclose(handle);
	}

	handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);

	struct battlebadgers_s *modscene = malloc(sizeof(struct battlebadgers_s));
	void(*test_init)(struct battlebadgers_s *scene, struct engine_s *) = dlsym(handle, "battlebadgers_init");
	test_init(modscene, engine);

	engine_setscene(engine, (struct scene_s *)modscene);

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
					window_resize(engine, event.window.data1, event.window.data2);
				}
				break;
			case SDL_KEYDOWN: {
				// SDL_KeyboardEvent *key_event = (SDL_KeyboardEvent *)&event;
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			{
				//SDL_MouseButtonEvent *mouse_event = (SDL_MouseButtonEvent *)&event;
				break;
			}
		}

		// TODO: only emit this in debug?
		if (event.type == USR_EVENT_RELOAD) {
			engine_setscene_dll(engine, "./scene_game.so");
		} else if (event.type == USR_EVENT_NOTIFY) {
			for (int i = 0; i < stbds_arrlen(engine->on_notify_callbacks); ++i) {
				engine->on_notify_callbacks[i](engine);
			}
		}
	}
}

void engine_draw(struct engine_s *engine) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	
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

