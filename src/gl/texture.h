#ifndef TEXTURE_H
#define TEXTURE_H

#include <SDL_opengles2.h>

typedef struct texture_s texture_t;
struct texture_s {
	GLuint texture;

	unsigned int width, height;
};

#define TEXTURE_SETTINGS_INIT     \
	{                             \
		.filter_min = GL_NEAREST, \
		.filter_mag = GL_NEAREST, \
		.wrap_s = GL_REPEAT,      \
		.wrap_t = GL_REPEAT,      \
		.gen_mipmap = 0,          \
		.flip_y = 0,              \
	}

struct texture_settings_s {
	GLint filter_min;
	GLint filter_mag;
	GLint wrap_s;
	GLint wrap_t;
	int gen_mipmap;
	int flip_y;
};

void texture_init(struct texture_s *texture, int width, int height, struct texture_settings_s *settings);
void texture_init_from_image(struct texture_s *texture, const char *source_path, struct texture_settings_s *settings);
void texture_destroy(struct texture_s *texture);

// GLuint texture_from_image(const char *source_path, struct texture_settings_s *settings);

#endif

