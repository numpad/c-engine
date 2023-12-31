#ifndef UGUI_H
#define UGUI_H

typedef struct engine_s engine_t;

void ugui_mainmenu_bar(engine_t *);
void ugui_mainmenu_bookmark(engine_t *, float x);
void ugui_mainmenu_icon(engine_t *, float x, const char *label, int icon, int font, float active);

#endif

