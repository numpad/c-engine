#ifndef ENGINE_EVENT_H
#define ENGINE_EVENT_H

enum engine_event_type {
	ENGINE_EVENT_WINDOW_RESIZED,
	ENGINE_EVENT_MAX,
};

struct engine_event {
	enum engine_event_type type;
	union {
		struct {
			int new_width, new_height;
		} window_resized;
	} data;
};

#endif

