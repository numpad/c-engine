#ifndef UGUI_H
#define UGUI_H

#include <nanovg.h>

struct ugui_element_s;

typedef void(*ugui_element_draw_fn)(NVGcontext *, struct ugui_element_s element);

struct ugui_element_s {
	float x, y, width, height;

	float press;

	ugui_element_draw_fn *draw;
};

#endif

