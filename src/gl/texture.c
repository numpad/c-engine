#include "texture.h"

#include <SDL.h>
#include <SDL_opengles2.h>
#include <stb_image.h>

void texture_init(struct texture_s *texture) {
	texture->texture = 0;
	texture->width = 0;
	texture->height = 0;
}

void texture_init_from_image(struct texture_s *texture, const char *source_path, struct texture_settings_s *settings) {
	texture_init(texture);

	int tw, th, tn;
	stbi_set_flip_vertically_on_load(settings ? settings->flip_y : 1);
	unsigned char *tpixels = stbi_load(source_path, &tw, &th, &tn, 0);
	stbi_set_flip_vertically_on_load(0);
	if (tpixels != NULL) {
		texture->width = tw;
		texture->height = th;

		glGenTextures(1, &texture->texture);
		glBindTexture(GL_TEXTURE_2D, texture->texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (settings ? settings->filter_min : GL_LINEAR));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (settings ? settings->filter_mag : GL_NEAREST));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (settings ? settings->wrap_s : GL_REPEAT));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (settings ? settings->wrap_t : GL_REPEAT));

		GLenum format;
		switch (tn) {
		case 1:
			format = GL_RED_EXT;
			break;
		case 2:
			format = GL_RG_EXT;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 4:
			format = GL_RGBA;
			break;
		default:
			SDL_Log("could not determine channels in image \"%s\"...", source_path);
			return;
		};
		
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

