#include "engine.h"
#include <assert.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#include "gl/opengles3.h"
#define NANOVG_GLES3_IMPLEMENTATION
#include <nanovg_gl.h>
#undef NANOVG_GLES3_IMPLEMENTATION
#include <stb_ds.h>
#include <cJSON.h>
#include "event.h"
#include "scenes/intro.h"
#include "scenes/menu.h"
#include "scenes/battle.h"
#include "scenes/brickbreaker.h"
#include "gl/shader.h"
#include "gui/console.h"
#include "net/message.h"
#include "util/util.h"

static Uint32 USR_EVENT_RELOAD = ((Uint32)-1);
static Uint32 USR_EVENT_NOTIFY = ((Uint32)-1);
static Uint32 USR_EVENT_GOBACK = ((Uint32)-1);

static void on_window_resized(struct engine *engine, int w, int h);
static void engine_poll_events(struct engine *engine);
static void engine_gameserver_receive(struct engine *engine);

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


struct engine *engine_new(int argc, char **argv) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL: %s.\n", SDL_GetError());
		return NULL;
	}

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 4, 2048) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL_mixer: %s.\n", Mix_GetError());
		return NULL;
	}

	// OpenGL config
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	// TODO: Keep MSAA for forward rendering?
	// TODO: Enabling MSAA breaks blitting gbuffer depth to default depth buffer...
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

	struct engine *engine = calloc(1, sizeof(struct engine));
	engine->window_width = 420;
	engine->window_height = 834;
	engine->time_elapsed = 0.0f;
	engine->dt = 0.0f;
	engine->window = SDL_CreateWindow("Demo - c-engine",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			engine->window_width, engine->window_height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
	assert(engine->window != NULL && "Failed creating SDL window");
	engine->window_id = SDL_GetWindowID(engine->window);
	engine->on_notify_callbacks = NULL;
	engine->scene = NULL;
	engine->console = malloc(sizeof(struct console_s));
	engine->gameserver_ip.host = engine->gameserver_ip.port = 0;
	engine->gameserver_tcp = NULL;
	engine->gameserver_socketset = NULL;
	engine->console_visible = 1;
	engine->freetype = NULL;
	console_init(engine->console);

	if (SDLNet_Init() < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed initializing SDL_net.\n");
		return NULL;
	}

	// OpenGL
	engine->gl_ctx = SDL_GL_CreateContext(engine->window);
	if (SDL_GL_MakeCurrent(engine->window, engine->gl_ctx) != 0) {
		console_log_ex(engine, CONSOLE_MSG_ERROR, 4.0f, "Failed making context current: %s", SDL_GetError());
	}

	if (SDL_GL_SetSwapInterval(1) != 0) {
		console_log_ex(engine, CONSOLE_MSG_ERROR, 4.0f, "Failed enabling v-sync: %s", SDL_GetError());
	}

	// Do some OpenGL checks
	GLint max_draw_buffers;
	glGetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &max_draw_buffers);
	if (max_draw_buffers < 4) {
		console_log_ex(engine, CONSOLE_MSG_ERROR, 8.0f, "Not enough draw_buffers: %d", max_draw_buffers);
	}
	GLint max_ubo_components;
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_ubo_components);
	if (max_ubo_components < 16) {
		console_log_ex(engine, CONSOLE_MSG_ERROR, 8.0f, "Not enough UBO components: %d", max_ubo_components);
	}

	// libs
	engine->vg = nvgCreateGLES3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
	engine->font_default_bold = nvgCreateFont(engine->vg, "Inter Regular", "res/font/Inter-Bold.ttf");
	engine->font_monospace = nvgCreateFont(engine->vg, "NotoSansMono", "res/font/NotoSansMono-Regular.ttf");

	// custom events
	USR_EVENT_RELOAD = SDL_RegisterEvents(1);
	USR_EVENT_NOTIFY = SDL_RegisterEvents(2);
	USR_EVENT_GOBACK = SDL_RegisterEvents(3);
#ifdef __unix__
	signal(SIGUSR1, on_sigusr1);
	signal(SIGUSR2, on_sigusr2);
#endif

	// font system
	if (FT_Init_FreeType(&engine->freetype) != 0) {
		fprintf(stderr, "error: failed initializing freetype!\n");
		console_log_ex(engine, CONSOLE_MSG_ERROR, 4.0f, "Failed initializing FreeType");
	}

	// camera
	glm_mat4_identity(engine->u_view);
	on_window_resized(engine, engine->window_width, engine->window_height);
	// Shader global data
	engine->shader_global_data.periodic_time = 0.0f;
	shader_ubo_init(&engine->shader_global_ubo, sizeof(engine->shader_global_data),
			(const void *)&engine->shader_global_data);

	// scene
	if (is_argv_set(argc, argv, "--scene=menu")) {
		console_log(engine, "Skipping Splash-Screen");
		struct scene_menu_s *menu = malloc(sizeof(struct scene_menu_s));
		scene_menu_init(menu, engine);
		engine_setscene(engine, (struct scene_s *)menu);
	} else if (is_argv_set(argc, argv, "--scene=battle")) {
		struct scene_battle_s *battle = malloc(sizeof(struct scene_battle_s));
		scene_battle_init(battle, engine);
		engine_setscene(engine, (struct scene_s *)battle);
	} else {
		struct scene_intro_s *intro = malloc(sizeof(struct scene_intro_s));
		scene_intro_init(intro, engine);
		engine_setscene(engine, (struct scene_s *)intro);
	}

	return engine;
}

int engine_destroy(struct engine *engine) {
	// scene
	engine_setscene(engine, NULL);

	// other
	nvgDeleteGLES3(engine->vg);
	stbds_arrfree(engine->on_notify_callbacks);
	console_destroy(engine->console);
	free(engine->console);

	// fonts
	FT_Done_FreeType(engine->freetype);
	engine->freetype = NULL;

	// windowing
	SDL_DestroyWindow(engine->window);
	SDL_GL_DeleteContext(engine->gl_ctx);
	shader_ubo_destroy(&engine->shader_global_ubo);

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

static void on_window_resized(struct engine *engine, int w, int h) {
	engine->window_width = w;
	engine->window_height = h;
	engine->window_aspect = h / (float)w;

	// highdpi scaling
	SDL_GL_GetDrawableSize(engine->window, &engine->window_highdpi_width, &engine->window_highdpi_height);
	float window_dpi_scale_x = (float)engine->window_highdpi_width / engine->window_width;
	float window_dpi_scale_y = (float)engine->window_highdpi_height / engine->window_height;

	engine->window_pixel_ratio = fmaxf(window_dpi_scale_x, window_dpi_scale_y);

	// apply changes
	glViewport(0, 0, w * window_dpi_scale_x, h * window_dpi_scale_y);
	glm_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f, engine->u_projection);
	//glm_ortho(-1.0f, 1.0f, -1.0f * engine->window_aspect, 1.0f * engine->window_aspect, -1.0f, 1.0f, engine->u_projection);
	
	scene_on_callback(engine->scene, engine, (struct engine_event){ .type = ENGINE_EVENT_WINDOW_RESIZED });
}


//
// scene handling
//

void engine_setscene(struct engine *engine, struct scene_s *new_scene) {
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
void engine_setscene_dll(struct engine *engine, const char *filename) {
	engine_setscene(engine, NULL);

	static void *handle = NULL;
	if (handle != NULL) {
		dlclose(handle);
	}

	handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);

#define SCENE_STRUCT scene_battle_s
#define SCENE_INIT_FN "scene_battle_init"
	struct SCENE_STRUCT *modscene = malloc(sizeof(*modscene));
	void(*init_fn)(struct SCENE_STRUCT *scene, struct engine *) = dlsym(handle, SCENE_INIT_FN);
	init_fn(modscene, engine);
#undef SCENE_STRUCT
#undef SCENE_INIT_FN

	engine_setscene(engine, (struct scene_s *)modscene);

}
#else
void engine_setscene_dll(struct engine *engine, const char *filename) {
	fprintf(stderr, "dll loading disabled for non-debug builds!\n");
}
#endif

//
// networking
//

int engine_gameserver_connect(struct engine *engine, const char *address) {
	assert(engine->gameserver_ip.host == 0);
	assert(engine->gameserver_ip.port == 0);

	if (engine->gameserver_tcp != NULL) {
		fprintf(stderr, "Already connected to gameserver.");
		return 1;
	}

	const int port = 9124;
	if (SDLNet_ResolveHost(&engine->gameserver_ip, address, port) < 0) {
		engine->gameserver_ip.host = engine->gameserver_ip.port = 0;
		fprintf(stderr, "Could not resolve host: \"%s\"...", address);
		return 1;
	}

	engine->gameserver_tcp = SDLNet_TCP_Open(&engine->gameserver_ip);
	if (engine->gameserver_tcp == NULL) {
		engine->gameserver_ip.host = engine->gameserver_ip.port = 0;
		fprintf(stderr, "Failed connecting to gameserver...");
		return 1;
	}

	engine->gameserver_socketset = SDLNet_AllocSocketSet(1); // we just connect to the server
	if (SDLNet_TCP_AddSocket(engine->gameserver_socketset, engine->gameserver_tcp) < 0) {
		assert("SocketSet is already full...");
		fprintf(stderr, "Socketset already full?");
		return 1;
	}

	return 0;
}

void engine_gameserver_disconnect(struct engine *engine) {
	engine->gameserver_ip.host = engine->gameserver_ip.port = 0;

	if (engine->gameserver_tcp != NULL) {
		if (engine->scene != NULL) {
			scene_on_message(engine->scene, engine, &(struct message_header){ .type = MSG_DISCONNECTED });
		}

		// socketset
		SDLNet_TCP_DelSocket(engine->gameserver_socketset, engine->gameserver_tcp);
		SDLNet_FreeSocketSet(engine->gameserver_socketset);
		engine->gameserver_socketset = NULL;
		// actual socket
		SDLNet_TCP_Close(engine->gameserver_tcp);
		engine->gameserver_tcp = NULL;
	}
}

void engine_gameserver_send(struct engine *engine, struct message_header *msg) {
	assert(engine != NULL);
	assert(msg != NULL);
	if (engine->gameserver_tcp == NULL) {
		return;
	}

	// serialize
	cJSON *json = pack_message(msg);
	char *data = cJSON_PrintUnformatted(json);
	const size_t data_len = strlen(data);
	cJSON_Delete(json);

	// send data
	const int result = SDLNet_TCP_Send(engine->gameserver_tcp, data, data_len);
	if (result != (int)data_len) {
		// TODO: Retry?
		fprintf(stderr, "Only sent %d of %zu bytes, that means we disconnect...\n", result, data_len);
		engine_gameserver_disconnect(engine);
	}

	free(data);
}

// main loop
void engine_update(struct engine *engine, double dt) {
	// poll server
	if (engine->gameserver_tcp != NULL) {
		engine_gameserver_receive(engine);
	}
	
	// poll events
	engine_poll_events(engine);

	// update shader global data
	engine->shader_global_data.periodic_time += dt * 0.33333;
	if (engine->shader_global_data.periodic_time >= GLM_PI) {
		engine->shader_global_data.periodic_time -= GLM_PI;
	}
	shader_ubo_update(&engine->shader_global_ubo, sizeof(engine->shader_global_data),
			(const void *)&engine->shader_global_data);


	// update
	console_update(engine->console, engine, dt);
	scene_update(engine->scene, engine, dt);
}

void engine_draw(struct engine *engine) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	
	nvgBeginFrame(engine->vg, engine->window_width, engine->window_height, engine->window_pixel_ratio);

	// run scene
	scene_draw(engine->scene, engine);

	if (engine->console_visible != 0) {
		console_draw(engine->console, engine);
	}

#ifdef DEBUG
	STATIC_RINGBUFFER(float, delta_times, 10);
	RINGBUFFER_APPEND(delta_times, engine->dt);
	static float avg = -1.0f;

	if (delta_times.len == delta_times.capacity) {
		float sum = 0.0f;
		while (delta_times.len > 0) {
			sum += RINGBUFFER_CONSUME(delta_times);
		}
		avg = sum / delta_times.capacity;
	}

	if (avg >= 0.0f) {

		// display debug_info
		char debug_info[128];
		//snprintf(debug_info, 128, "dt=%.1fms / FPS=%.0f [%.1fs total]", engine->dt * 1000.0f, 1.0 / engine->dt, engine->time_elapsed);
		snprintf(debug_info, 128, "dt=%.1fms / FPS=%.0f [%.1fs total]", avg * 1000.0f, 1.0 / avg, engine->time_elapsed);
		//printf("dt = %.5fms / %.0f (Total: %.2f)\n", dt, 1.0 / dt, engine->time_elapsed);

		vec4 bounds;
		nvgTextAlign(engine->vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		nvgFontFaceId(engine->vg, engine->font_monospace);
		nvgFontBlur(engine->vg, 0.0f);
		nvgFontSize(engine->vg, 12.0f);
		nvgTextLetterSpacing(engine->vg, 1.0f);
		nvgTextLineHeight(engine->vg, 1.0f);
		nvgTextBounds(engine->vg, 0.0f, 0.0f, debug_info, NULL, bounds);

		// bg
		nvgBeginPath(engine->vg);
		nvgRect(engine->vg, engine->window_width - bounds[2], engine->window_height - bounds[3], bounds[2], bounds[3]);
		nvgFillColor(engine->vg, nvgRGBAf(0, 0, 0, 0.65f));
		nvgFill(engine->vg);

		// text
		nvgBeginPath(engine->vg);
		nvgFillColor(engine->vg, nvgRGBf(1.0f, 1.0f, 0.0f));
		nvgText(engine->vg, engine->window_width - bounds[2], engine->window_height - bounds[3], debug_info, NULL);
	}
#endif

	nvgEndFrame(engine->vg);

	SDL_GL_SwapWindow(engine->window);
}

void engine_enter_mainloop(struct engine *engine) {
	Uint64 current_time = SDL_GetPerformanceCounter();
	while (engine->scene != NULL) {
		const Uint64 new_time = SDL_GetPerformanceCounter();

		const Uint64 elapsed_time = new_time - current_time;
		engine->dt = elapsed_time / (double)SDL_GetPerformanceFrequency();
		current_time = new_time;
		
		engine_update(engine, engine->dt);
		engine->time_elapsed += engine->dt;

		engine_draw(engine);
	}
}

// TODO: merge with engine_enter_mainloop
void engine_mainloop_emcc(void *engine_ptr) {
	struct engine *engine = engine_ptr;

	static Uint64 current_time = -1;
	if (current_time == (Uint64)-1) {
		current_time = SDL_GetPerformanceCounter();
	}
	const Uint64 new_time = SDL_GetPerformanceCounter();

	const Uint64 elapsed_time = new_time - current_time;
	engine->dt = elapsed_time / (double)SDL_GetPerformanceFrequency();
	current_time = new_time;

	engine_update(engine, engine->dt);
	engine->time_elapsed += engine->dt;

	engine_draw(engine);
}

// event polling
static void engine_poll_events(struct engine *engine) {
	struct input_drag_s prev_input_drag = engine->input_drag;

	if (prev_input_drag.state == INPUT_DRAG_END) {
		engine->input_drag.state = INPUT_DRAG_NONE;
	} else if (prev_input_drag.state == INPUT_DRAG_BEGIN) {
		engine->input_drag.state = INPUT_DRAG_IN_PROGRESS;
	}

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
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
				break;
			case SDL_TEXTEDITING:
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				SDL_KeyboardEvent key_event = event.key;
				scene_on_callback(engine->scene, engine, (struct engine_event){ .type = ENGINE_EVENT_KEY, .data.key = key_event });
				// TODO: pop scene stack, or other data structure?
				if (key_event.type == SDL_KEYUP && key_event.keysym.sym == SDLK_ESCAPE) {
					// TODO: open menu first / send event to scene?
					on_siggoback();
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
			engine_setscene_dll(engine, "./hotreload.so");
		} else if (event.type == USR_EVENT_NOTIFY) {
			for (int i = 0; i < stbds_arrlen(engine->on_notify_callbacks); ++i) {
				engine->on_notify_callbacks[i](engine);
			}
		} else if (event.type == USR_EVENT_GOBACK) {
			console_log(engine, "← Back");

			// TODO: only switch if we're not in menu? need to be able to check scene type
			// TODO: notify current scene about this
			struct scene_menu_s *menu_scene = malloc(sizeof(struct scene_menu_s));
			scene_menu_init(menu_scene, engine);
			engine_setscene(engine, (struct scene_s *)menu_scene);
		}
	}
}

// receive & parse messages
static void propagate_received_message(struct engine *engine, const char *data, size_t data_len) {
	// we received something, but maybe it is no json/valid message?
	cJSON *json = cJSON_ParseWithLength(data, data_len);
	if (json != NULL) {
		// it may be valid json, but a valid message?
		struct message_header *header = unpack_message(json);
		if (header != NULL) {
			printf("Received a %s\n", message_type_to_name(header->type));
			scene_on_message(engine->scene, engine, header);
			free_message(json, header);
		} else {
			cJSON_Delete(json);
		}
	}
}

static void engine_gameserver_receive(struct engine *engine) {
	assert(engine != NULL);

	const int readable_sockets = 1;
	// Check if sockets are readable.
	if (SDLNet_CheckSockets(engine->gameserver_socketset, 0) == readable_sockets) {
		static char data[512] = {0};

		// TODO: we handle reading multiple objects in a single recv, but not partial objects.
		const int data_len = SDLNet_TCP_Recv(engine->gameserver_tcp, &data, 511);
		if (data_len > 0) {
			// handle multiple json objects in a single message
			const char *begin = data;
			while (begin != NULL && begin < data + data_len) {
				// TODO: this is garbage. find the end of this object
				const char *end = str_match_bracket(begin, data_len, '{', '}');
				if (end == NULL) break;

				const int len = end - begin + 1;

				propagate_received_message(engine, begin, len);

				begin += len;
			}
			assert(begin == data + data_len);

		} else {
			// TODO: why did we recieve nothing?
			printf("TCP_Recv failure, got 0 bytes...\n");
			// TODO: if we dont disconnect this will trigger continously in browser
			engine_gameserver_disconnect(engine);
		}
	}
}
