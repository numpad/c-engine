#ifndef UGUI_H
#define UGUI_H

#include <stddef.h>

struct ugui_ctx_s;

//
// EVENTS

enum UGUI_EVENT_TYPE {
	UGUI_EV_UNKNOWN,
	UGUI_EV_BUTTONPRESS, UGUI_EV_BUTTONDOWN,UGUI_EV_BUTTONRELEASE,
};

typedef void(*ugui_event_callback)(struct ugui_ctx_s *ctx, enum UGUI_EVENT_TYPE event_type);

//
// CONTEXT

struct ugui_ctx_s {
	ugui_event_callback *on_event;
	void *userdata;
};

//
// ITEMS

struct ugui_item_s {
	float x, y, w, h;
};

struct ugui_button_s {
	struct ugui_item_s base;

	float press;
};

void ugui_ctx_init(struct ugui_ctx_s *ctx);

#endif

