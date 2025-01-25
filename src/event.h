#ifndef ENGINE_EVENT_H
#define ENGINE_EVENT_H

#include <SDL.h>

enum engine_event_type {
	ENGINE_EVENT_WINDOW_RESIZED,
	ENGINE_EVENT_KEY,
	ENGINE_EVENT_CLOSE_SCENE,
	ENGINE_EVENT_MAX,
};

struct engine_event {
	enum engine_event_type type;
	union {
		SDL_KeyboardEvent key;
	} data;
};

#endif

