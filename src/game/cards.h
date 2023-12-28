#ifndef CARDS_H
#define CARDS_H

#include <SDL_opengles2.h>
#include "gl/vbuffer.h"
#include "gl/texture.h"
#include "gl/shader.h"

struct cardrenderer_s {
	shader_t shader;
	vbuffer_t vbo;
	texture_t texture_atlas;
};

void cardrenderer_init(struct cardrenderer_s *, const char *tileset);
void cardrenderer_destroy(struct cardrenderer_s *);

#endif

