#ifndef UGUI_H
#define UGUI_H

#include <nanovg.h>

struct engine;

void ugui_mainmenu_bar(struct engine *);
void ugui_mainmenu_bookmark(struct engine *, float x);
void ugui_mainmenu_icon(struct engine *, float x, const char *label, int icon, int font, float active);

// Windows & Modals
typedef struct rendered_modal {
	NVGcontext *vg;
	float x;
	float y;
	float width;
	float height;
} rendered_modal_t;

rendered_modal_t ugui_modal_begin(struct engine *);
void ugui_modal_end(rendered_modal_t);

// UI Elements
void ugui_mainmenu_button(struct engine *, float x, float y, float w, float h, const char *text1, const char *text2, const char *subtext, int font, NVGcolor color_bg, NVGcolor color_bg_darker, NVGcolor color_text_outline, float is_pressed);
void ugui_mainmenu_checkbox(struct engine *, float x, float y, float w, float h, int font, const char *text, float is_checked);

#endif

