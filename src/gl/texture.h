#ifndef TEXTURE_H
#define TEXTURE_H

#include <SDL_opengles2.h>

typedef struct texture_s texture_t;
struct texture_s {
	GLuint texture;

	unsigned int width, height;
};

struct texture_settings_s {
	GLint filter_min;
	GLint filter_mag;
	GLint wrap_s;
	GLint wrap_t;
	int gen_mipmap;
};

void texture_init(struct texture_s *texture);
void texture_init_from_image(struct texture_s *texture, const char *source_path, struct texture_settings_s *settings);
void texture_destroy(struct texture_s *texture);

// GLuint texture_from_image(const char *source_path, struct texture_settings_s *settings);

#endif

