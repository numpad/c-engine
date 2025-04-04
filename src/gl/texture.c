#include "texture.h"

#include <assert.h>
#include <SDL.h>
#include <SDL_opengles2.h>
#include <stb_image.h>
#include "util/util.h"

static void set_texparams_from_settings(GLuint target, struct texture_settings_s *settings) {
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (settings ? settings->filter_min : GL_LINEAR));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (settings ? settings->filter_mag : GL_NEAREST));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (settings ? settings->wrap_s : GL_REPEAT));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (settings ? settings->wrap_t : GL_REPEAT));
}

static int channels_from_format(GLint format) {
	switch (format) {
		case GL_RED_EXT:
		case GL_ALPHA:  return 1;
		case GL_RG_EXT: return 2;
		case GL_RGB:    return 3;
		case GL_RGBA:   return 4;
	default:
		fprintf(stderr, "[warn] no information about number of channels in texture format \"%d\"... assuming 4.\n", format);
		break;
	};
	return 4;
}

void texture_init(struct texture_s *texture, int width, int height, struct texture_settings_s *settings) {
	assert(texture != NULL);
	assert(width > 0);
	assert(height > 0);

	// format & internalformat need to match in es2
	GLint format = (settings != NULL) ? settings->internal_format : GL_RGBA;

	texture->width = width;
	texture->height = height;
	texture->internal_format = format;

	glGenTextures(1, &texture->texture);
	glBindTexture(GL_TEXTURE_2D, texture->texture);
	set_texparams_from_settings(GL_TEXTURE_2D, settings);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
	GL_CHECK_ERROR();
	glBindTexture(GL_TEXTURE_2D, 0);
}

void texture_init_from_image(struct texture_s *texture, const char *source_path, struct texture_settings_s *settings) {
	int tw, th, tn;
	stbi_set_flip_vertically_on_load(settings ? settings->flip_y : 1);
	unsigned char *tpixels = stbi_load(source_path, &tw, &th, &tn, 0);
	stbi_set_flip_vertically_on_load(0);

	assert(tpixels != NULL);

	if (tpixels != NULL) {
		texture->width = tw;
		texture->height = th;

		glGenTextures(1, &texture->texture);
		glBindTexture(GL_TEXTURE_2D, texture->texture);
		set_texparams_from_settings(GL_TEXTURE_2D, settings);
		GLenum format;
		switch (tn) {
		case 1: format = GL_RED_EXT; break;
		case 2: format = GL_RG_EXT; break;
		case 3: format = GL_RGB; break;
		case 4: format = GL_RGBA; break;
		default:
			fprintf(stderr, "[warn] could not determine channels of image \"%s\"...\n", source_path);
			return;
		};
		
		texture->internal_format = format;
		glTexImage2D(GL_TEXTURE_2D, 0, format, tw, th, 0, format, GL_UNSIGNED_BYTE, tpixels);

		if (settings != NULL && settings->gen_mipmap) {
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		stbi_image_free(tpixels);
	}
}

void texture_init_from_memory(struct texture_s *texture, unsigned int data_len, const unsigned char *data, struct texture_settings_s *settings) {
	int tw, th, tn;
	stbi_set_flip_vertically_on_load(settings ? settings->flip_y : 1);
	unsigned char *tpixels = stbi_load_from_memory(data, data_len, &tw, &th, &tn, 0);
	stbi_set_flip_vertically_on_load(0);

	assert(tpixels != NULL);

	if (tpixels != NULL) {
		texture->width = tw;
		texture->height = th;

		glGenTextures(1, &texture->texture);
		glBindTexture(GL_TEXTURE_2D, texture->texture);
		set_texparams_from_settings(GL_TEXTURE_2D, settings);
		GLenum format;
		switch (tn) {
		case 1: format = GL_RED_EXT; break;
		case 2: format = GL_RG_EXT; break;
		case 3: format = GL_RGB; break;
		case 4: format = GL_RGBA; break;
		default:
			fprintf(stderr, "[warn] could not determine channels of in-memory image...\n");
			return;
		};
		
		texture->internal_format = format;
		glTexImage2D(GL_TEXTURE_2D, 0, format, tw, th, 0, format, GL_UNSIGNED_BYTE, tpixels);

		if (settings != NULL && settings->gen_mipmap) {
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		stbi_image_free(tpixels);
	}
}

void texture_destroy(struct texture_s *texture) {
	glDeleteTextures(1, &texture->texture);
	texture->width = 0;
	texture->height = 0;
}

void texture_clear(struct texture_s *texture) {
	assert(texture != NULL);

	int channels = channels_from_format(texture->internal_format);
	GLubyte zero_data[texture->width * texture->height * channels];
	memset(zero_data, 0, sizeof(zero_data));

	glBindTexture(GL_TEXTURE_2D, texture->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, texture->internal_format, texture->width, texture->height, 0, texture->internal_format, GL_UNSIGNED_BYTE, zero_data);
	GL_CHECK_ERROR();
	glBindTexture(GL_TEXTURE_2D, 0);
}

