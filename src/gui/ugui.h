#ifndef UGUI_H
#define UGUI_H

#include <nanovg.h>

typedef struct engine_s engine_t;

void ugui_mainmenu_bar(engine_t *);
void ugui_mainmenu_bookmark(engine_t *, float x);
void ugui_mainmenu_icon(engine_t *, float x, const char *label, int icon, int font, float active);

void ugui_mainmenu_button(engine_t *, float x, float y, float w, float h, const char *text1, const char *text2, const char *subtext, int font, NVGcolor color_bg, NVGcolor color_bg_darker, NVGcolor color_text_outline, float is_pressed);

void ugui_mainmenu_checkbox(engine_t *, float x, float y, float w, float h, int font, const char *text, float is_checked);

#endif

