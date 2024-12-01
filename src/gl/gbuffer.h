#ifndef GBUFFER_H
#define GBUFFER_H

#include <SDL_opengles2.h>
#include "engine.h"
#include "gl/shader.h"

enum gbuffer_texture {
	GBUFFER_TEXTURE_ALBEDO,
	GBUFFER_TEXTURE_POSITION,
	GBUFFER_TEXTURE_NORMAL,
	GBUFFER_TEXTURE_MAX,
};

struct gbuffer {
	GLuint framebuffer;
	GLuint renderbuffer;
	GLuint textures[GBUFFER_TEXTURE_MAX];
	shader_t shader;

	GLuint textures_internalformats[GBUFFER_TEXTURE_MAX];
	GLenum textures_type[GBUFFER_TEXTURE_MAX];
};

void gbuffer_init(struct gbuffer *, struct engine *);
void gbuffer_destroy(struct gbuffer *);
void gbuffer_resize(struct gbuffer *, int width, int height);

void gbuffer_bind(struct gbuffer);
void gbuffer_unbind(struct gbuffer);

void gbuffer_display(struct gbuffer, struct engine *);

#endif

