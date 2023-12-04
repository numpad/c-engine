#include "engine.h"
#include <assert.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#define NANOVG_GLES2_IMPLEMENTATION
#include <nanovg_gl.h>
#undef NANOVG_GLES2_IMPLEMENTATION
#include <stb_ds.h>
#include <nuklear.h>
#include <nuklear_sdl_gles2.h>
#include "scenes/intro.h"
#include "scenes/menu.h"
#include "scenes/experiments.h"
#include "gui/console.h"

static Uint32 USR_EVENT_RELOAD = ((Uint32)-1);
static Uint32 USR_EVENT_NOTIFY = ((Uint32)-1);
static Uint32 USR_EVENT_GOBACK = ((Uint32)-1);

static void on_window_resized(struct engine_s *engine, int w, int h);

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
void on_siggoback(void) {
	if (USR_EVENT_GOBACK != ((Uint32)-1)) {
		SDL_Event event;
		SDL_memset(&event, 0, sizeof(event)); /* or SDL_zero(event) */
		event.type = USR_EVENT_GOBACK;
		event.user.code = 0;
		event.user.data1 = NULL;
		event.user.data2 = NULL;
		SDL_PushEvent(&event);
	}
	// dont need to re-register this signal
}


struct engine_s *engine_new(void) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL: %s.\n", SDL_GetError());
		return NULL;
	}

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL_mixer: %s.\n", Mix_GetError());
		return NULL;
	}

	struct engine_s *engine = malloc(sizeof(struct engine_s));
	engine->window_width = 550;
	engine->window_height = 800;
	engine->time_elapsed = 0.0f;
	engine->window = SDL_CreateWindow("Soil Soldiers", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, engine->window_width, engine->window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
	engine->window_id = SDL_GetWindowID(engine->window);
	engine->on_notify_callbacks = NULL;
	engine->scene = NULL;
	engine->console = malloc(sizeof(struct console_s));
	engine->gameserver_ip.host = engine->gameserver_ip.port = 0;
	engine->gameserver_tcp = NULL;
	engine->gameserver_socketset = NULL;
	console_init(engine->console);

	if (engine->window == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL window.\n");
		return NULL;
	}

	if (SDLNet_Init() < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL_net.\n");
		return NULL;
	}

	// OpenGL config
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	SDL_GL_SetSwapInterval(1);
	engine->gl_ctx = SDL_GL_CreateContext(engine->window);
	
	// libs
	engine->vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

	// custom events
	USR_EVENT_RELOAD = SDL_RegisterEvents(1);
	USR_EVENT_NOTIFY = SDL_RegisterEvents(2);
	USR_EVENT_GOBACK = SDL_RegisterEvents(3);
#ifdef __unix__
	signal(SIGUSR1, on_sigusr1);
	signal(SIGUSR2, on_sigusr2);
#endif

	// init nuklear
	engine->nk = nk_sdl_init(engine->window);
	// init default font
	struct nk_font_atlas *atlas;
	nk_sdl_font_stash_begin(&atlas);
	nk_sdl_font_stash_end();

	// scene
	struct intro_s *intro = malloc(sizeof(struct intro_s));
	intro_init(intro, engine);
	engine_setscene(engine, (struct scene_s *)intro);

	on_window_resized(engine, engine->window_width, engine->window_height);
	glClearColor(0.06f, 0.0f, 0.10f, 1.0f);

	// camera
	glm_mat4_identity(engine->u_view);

	return engine;
}

int engine_destroy(struct engine_s *engine) {
	// scene
	engine_setscene(engine, NULL);

	// other
	nvgDeleteGLES2(engine->vg);
	stbds_arrfree(engine->on_notify_callbacks);
	console_destroy(engine->console);
	free(engine->console);
	nk_sdl_shutdown();

	// windowing
	SDL_DestroyWindow(engine->window);
	SDL_GL_DeleteContext(engine->gl_ctx);

	// net
	engine_gameserver_disconnect(engine);

	SDLNet_Quit();
	Mix_Quit();
	SDL_Quit();

	free(engine);
	return 0;
}

//
// settings
//
void engine_set_clear_color(float r, float g, float b) {
	glClearColor(r, g, b, 1.0f);
}

//
// system stuff
//

static void on_window_resized(struct engine_s *engine, int w, int h) {
	engine->window_width = w;
	engine->window_height = h;
	engine->window_aspect = h / (float)w;

	// highdpi scaling
	int draw_w, draw_h;
	SDL_GL_GetDrawableSize(engine->window, &draw_w, &draw_h);
	float window_dpi_scale_x = (float)draw_w / engine->window_width;
	float window_dpi_scale_y = (float)draw_h / engine->window_height;

	engine->window_pixel_ratio = fmaxf(window_dpi_scale_x, window_dpi_scale_y);

	// apply changes
	glViewport(0, 0, w * window_dpi_scale_x, h * window_dpi_scale_y);
	glm_ortho(-1.0f, 1.0f, -1.0f * engine->window_aspect, 1.0f * engine->window_aspect, -1.0f, 1.0f, engine->u_projection);

#ifdef DEBUG
	char msg[64];
	sprintf(msg, "Resized to %dx%d", w, h);
	console_add_message(engine->console, (struct console_msg_s) { .message = msg });
#endif
}


//
// scene handling
//

void engine_setscene(struct engine_s *engine, struct scene_s *new_scene) {
#ifdef DEBUG
	static char text[256];
	text[0] = '\0';
	sprintf(text, "Switching to scene %p", (void*)new_scene);
	console_add_message(engine->console, (struct console_msg_s){ text });
#endif

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

	struct scene_experiments_s *modscene = malloc(sizeof(struct scene_experiments_s));
	void(*test_init)(struct scene_experiments_s *scene, struct engine_s *) = dlsym(handle, "scene_experiments_init");
	test_init(modscene, engine);

	engine_setscene(engine, (struct scene_s *)modscene);

}
#else
void engine_setscene_dll(struct engine_s *engine, const char *filename) {
	fprintf(stderr, "dll loading disabled for non-debug builds!\n");
}
#endif

//
// networking
//

int engine_gameserver_connect(struct engine_s *engine, const char *address) {
	assert(engine->gameserver_ip.host == 0);
	assert(engine->gameserver_ip.port == 0);

	if (engine->gameserver_tcp != NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Already connected to gameserver.");
		return 1;
	}

	const int port = 9124;
	if (SDLNet_ResolveHost(&engine->gameserver_ip, address, port) < 0) {
		engine->gameserver_ip.host = engine->gameserver_ip.port = 0;
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not resolve host: \"%s\"...", address);
		return 1;
	}

	engine->gameserver_tcp = SDLNet_TCP_Open(&engine->gameserver_ip);
	if (engine->gameserver_tcp == NULL) {
		engine->gameserver_ip.host = engine->gameserver_ip.port = 0;
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed connecting to gameserver...");
		return 1;
	}

	engine->gameserver_socketset = SDLNet_AllocSocketSet(1); // we just connect to the server
	if (SDLNet_TCP_AddSocket(engine->gameserver_socketset, engine->gameserver_tcp) < 0) {
		assert("SocketSet is already full...");
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Socketset already full?");
		return 1;
	}

	return 0;
}

void engine_gameserver_disconnect(struct engine_s *engine) {
	engine->gameserver_ip.host = engine->gameserver_ip.port = 0;

	if (engine->gameserver_tcp != NULL) {
		// socketset
		SDLNet_TCP_DelSocket(engine->gameserver_socketset, engine->gameserver_tcp);
		SDLNet_FreeSocketSet(engine->gameserver_socketset);
		engine->gameserver_socketset = NULL;
		// actual socket
		SDLNet_TCP_Close(engine->gameserver_tcp);
		engine->gameserver_tcp = NULL;
	}
}

// main loop
void engine_update(struct engine_s *engine) {
	struct input_drag_s prev_input_drag = engine->input_drag;

	if (prev_input_drag.state == INPUT_DRAG_END) {
		engine->input_drag.state = INPUT_DRAG_NONE;
	} else if (prev_input_drag.state == INPUT_DRAG_BEGIN) {
		engine->input_drag.state = INPUT_DRAG_IN_PROGRESS;
	}

	// poll server
	if (engine->gameserver_tcp != NULL) {
		const int readable_sockets = 1;
		if (SDLNet_CheckSockets(engine->gameserver_socketset, 0) == readable_sockets) {
			static char data[512] = {0};
			
			const int bytes_recv = SDLNet_TCP_Recv(engine->gameserver_tcp, &data, 511);
			if (bytes_recv <= 0) {
				printf("TCP_Recv failure, got 0 bytes... Disconnecting.\n");
				engine_gameserver_disconnect(engine);
			} else {
				data[bytes_recv] = 0;
				printf("Received: [%d] '%.*s'\n", bytes_recv, bytes_recv, data);
			}
		}
	}
	
	// poll events
    nk_input_begin(engine->nk);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
        // TODO: stop event propagation when interacting with gui?
		nk_sdl_handle_event(&event);

		switch (event.type) {
			case SDL_QUIT:
				engine_setscene(engine, NULL);
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED /* && event.window.windowID == engine->window_id */) {
					on_window_resized(engine, event.window.data1, event.window.data2);
				}
				break;
			case SDL_TEXTINPUT:
				console_add_input_text(engine->console, event.text.text);
				break;
			case SDL_TEXTEDITING:

				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				SDL_KeyboardEvent *key_event = (SDL_KeyboardEvent *)&event;
				// pop scene stack
				if (key_event->type == SDL_KEYUP && key_event->keysym.sym == SDLK_ESCAPE) {
					// TODO
				}
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			{
				SDL_MouseButtonEvent *mouse_event = (SDL_MouseButtonEvent *)&event;
				if (mouse_event->state == SDL_PRESSED) {
					engine->input_drag.state = INPUT_DRAG_BEGIN;
					engine->input_drag.begin_x = mouse_event->x;
					engine->input_drag.begin_y = mouse_event->y;
				} else if (mouse_event->state == SDL_RELEASED) {
					engine->input_drag.state = INPUT_DRAG_END;
					engine->input_drag.end_x = mouse_event->x;
					engine->input_drag.end_y = mouse_event->y;
				}

				engine->input_drag.x = mouse_event->x;
				engine->input_drag.y = mouse_event->y;
				break;
			}
			case SDL_MOUSEMOTION: {
				SDL_MouseMotionEvent *motion = (SDL_MouseMotionEvent *)&event;
				if (prev_input_drag.state == INPUT_DRAG_BEGIN) {
					engine->input_drag.state = INPUT_DRAG_IN_PROGRESS;
				}

				if (prev_input_drag.state == INPUT_DRAG_IN_PROGRESS) {
					engine->input_drag.x = motion->x;
					engine->input_drag.y = motion->y;
				}
				break;
			}
		}

		// handle custom events

		if (event.type == USR_EVENT_RELOAD) {
			// only emit this in debug?
			engine_setscene_dll(engine, "./scene_game.so");
		} else if (event.type == USR_EVENT_NOTIFY) {
			for (int i = 0; i < stbds_arrlen(engine->on_notify_callbacks); ++i) {
				engine->on_notify_callbacks[i](engine);
			}
		} else if (event.type == USR_EVENT_GOBACK) {
			console_add_message(engine->console, (struct console_msg_s) { .message = "← Back" });

			// TODO: notify current scene about this
			struct menu_s *menu_scene = malloc(sizeof(struct menu_s));
			menu_init(menu_scene, engine);
			engine_setscene(engine, (struct scene_s *)menu_scene);
		}
	}
    nk_input_end(engine->nk);

	// update
	const float dt = 1.0f / 60.0f;
	engine->time_elapsed += dt;

	console_update(engine->console, engine, dt);
	scene_update(engine->scene, engine, dt);
}

void engine_draw(struct engine_s *engine) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	
	nvgBeginFrame(engine->vg, engine->window_width, engine->window_height, engine->window_pixel_ratio);

	// run scene
	scene_draw(engine->scene, engine);

	console_draw(engine->console, engine);

#ifdef DEBUG
	// display seconds
	char seconds[32];
	sprintf(seconds, "%.2fs", engine->time_elapsed);

	nvgBeginPath(engine->vg);
	nvgTextAlign(engine->vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
	nvgFillColor(engine->vg, nvgRGBf(0.0f, 0.0f, 0.0f));
	nvgFontBlur(engine->vg, 2.0f);
	nvgFontSize(engine->vg, 14.0f);
	nvgText(engine->vg, 4.0f, 4.0f, seconds, NULL);

	nvgBeginPath(engine->vg);
	nvgTextAlign(engine->vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
	nvgFillColor(engine->vg, nvgRGBf(1.0f, 1.0f, 0.6f));
	nvgFontBlur(engine->vg, 0.0f);
	nvgFontSize(engine->vg, 14.0f);
	nvgText(engine->vg, 3.0f, 3.0f, seconds, NULL);
#endif

	nk_sdl_render(NK_ANTI_ALIASING_ON, 512 * 1024, 128 * 1024);
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

