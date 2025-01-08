#ifndef CANVAS_H
#define CANVAS_H

#include <SDL_opengles2.h>
#include "gl/texture.h"

typedef struct canvas_s {
	GLuint fbo;
	texture_t texture;
	// Attachments
	// TODO: only one of these
	GLuint rbo_depth;
	GLuint rbo_stencil;
	GLuint rbo_depth_stencil;

	int pre_bind_viewport_width;
	int pre_bind_viewport_height;
} canvas_t;


typedef struct canvas_config_s {
	int has_depth;
	int has_stencil;
} canvas_config_t;

void canvas_init(canvas_t *, int width, int height, canvas_config_t *config);
void canvas_destroy(canvas_t *);

void canvas_bind(canvas_t *);
void canvas_unbind(canvas_t *);

#endif

