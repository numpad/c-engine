#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <SDL_net.h>
#include <SDL_opengles2.h>
#include <cglm/cglm.h>
#include <nanovg.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "scenes/scene.h"
#include "gl/shader.h"
#include "input.h"

//
// forward decls & typedefs
//

struct engine;
struct console_s;
struct message_header;
typedef void(*engine_callback_fn)(struct engine*);

//
// structs & enums
//

struct engine {
	// windowing
	SDL_Window *window;
	Uint32 window_id;
	SDL_GLContext gl_ctx;
	struct NVGcontext *vg;
	FT_Library freetype;
	
	// window
	int window_width, window_height;
	int window_highdpi_width, window_highdpi_height;
	float window_pixel_ratio;
	float window_aspect;

	// timing
	double time_elapsed;
	double dt;

	// scene management
	struct scene_s *scene;

	// hooks
	engine_callback_fn *on_notify_callbacks;

	// others
	struct console_s *console;
	struct input_drag_s input_drag;

	// server connection
	IPaddress gameserver_ip;
	TCPsocket gameserver_tcp;
	SDLNet_SocketSet gameserver_socketset;

	// rendering globals
	mat4 u_projection;
	mat4 u_view;
	struct {
		float time_elapsed;
		float _padding[3]; // TODO: is this really required for std140?
	} shader_global_data;
	struct shader_ubo shader_global_ubo;

	// dumb state
	int console_visible;
	int font_default_bold, font_monospace;
};

// init & cleanup
struct engine *engine_new(int argc, char **argv);
int engine_destroy(struct engine *engine);

// settings
void engine_set_clear_color(float r, float g, float b);

// scene handling
void engine_setscene(struct engine *engine, struct scene_s *scene);
void engine_setscene_dll(struct engine *engine, const char *filename);

// networking
int engine_gameserver_connect(struct engine *, const char *address);
void engine_gameserver_disconnect(struct engine *);
void engine_gameserver_send(struct engine *, struct message_header *msg);

// main loop
void engine_update(struct engine *engine, double dt);
void engine_draw(struct engine *engine);
void engine_enter_mainloop(struct engine *engine);
void engine_mainloop_emcc(void *engine);

#endif

