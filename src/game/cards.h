#ifndef CARDS_H
#define CARDS_H

#include <SDL_opengles2.h>
#include "gl/vbuffer.h"

struct cardrenderer_s {
	GLuint tileset;
	GLuint shader;
	struct vbuffer_s vbo;
};

void cardrenderer_init(struct cardrenderer_s *, const char *tileset);
void cardrenderer_destroy(struct cardrenderer_s *);

#endif

