#ifndef MAINMENU_H
#define MAINMENU_H

struct engine_s;

struct mainmenu_s {
	void *_dummy;
};


struct mainmenu_s *mainmenu_new();
int mainmenu_destroy(struct mainmenu_s *menu);

void mainmenu_draw(struct mainmenu_s *menu, struct engine_s *engine);

#endif

