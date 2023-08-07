#ifndef TEXTURE_H
#define TEXTURE_H

#include <SDL_opengles2.h>

struct texture_settings_s {
	GLint filter_min;
	GLint filter_mag;
	GLint wrap_s;
	GLint wrap_t;
};

GLuint texture_from_image(const char *source_path, struct texture_settings_s *settings);

#endif

