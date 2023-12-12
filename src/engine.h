#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <SDL_net.h>
#include <SDL_opengles2.h>
#include <cglm/cglm.h>
#include <nanovg.h>
#include "scenes/scene.h"
#include "input.h"

struct nk_context;
struct engine_s;
struct console_s;
struct message_header;
typedef void(*engine_callback_fn)(struct engine_s*);

struct engine_s {
	// windowing
	SDL_Window *window;
	Uint32 window_id;
	SDL_GLContext gl_ctx;
	struct NVGcontext *vg;
	struct nk_context *nk;
	
	// window
	int window_width, window_height;
	float window_pixel_ratio;
	float window_aspect;

	// timing
	double time_elapsed;

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

	// dumb state
	int console_visible;
};

// init & cleanup
struct engine_s *engine_new(void);
int engine_destroy(struct engine_s *engine);

// settings
void engine_set_clear_color(float r, float g, float b);

// scene handling
void engine_setscene(struct engine_s *engine, struct scene_s *scene);
void engine_setscene_dll(struct engine_s *engine, const char *filename);

// networking
int engine_gameserver_connect(struct engine_s *, const char *address);
void engine_gameserver_disconnect(struct engine_s *);
void engine_gameserver_send(struct engine_s *, struct message_header *msg);

// main loop
void engine_update(struct engine_s *engine);
void engine_draw(struct engine_s *engine);
void engine_mainloop(struct engine_s *engine);
void engine_mainloop_emcc(void *engine);

#endif

