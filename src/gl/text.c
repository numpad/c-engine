#include "text.h"

#include <stdio.h>
#include <assert.h>
#include <SDL_opengles2.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "gl/texture.h"
#include "gl/graphics2d.h"
#include "util/util.h"

static FT_Library library;
static FT_Face face;
static texture_t atlas;

#include <SDL2/SDL.h>

void text_global_init() {
	FT_Error error;

	error = FT_Init_FreeType(&library);
	if (error) {
		printf("error initializing freetype!\n");
		return;
	}

	error = FT_New_Face(library, "res/font/Inter-Regular.ttf", 0, &face);
	if (error) {
		printf("error creating freetype font!\n");
		return;
	}

	// TODO: handle dpi correctly.
	//       GetDisplayDPI doesnt really work on linux for me, reporting wrong DPI.
	//       The docs say SDL3 might fix this?
	//       Set_Pixel_Sizes works, but isn't crisp on highdpi.

	//float ddpi, hdpi, vdpi;
	//assert(SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0);
	//printf("Diagonal DPI: %f, Horizontal DPI: %f, Vertical DPI: %f", ddpi, hdpi, vdpi);
	//error = FT_Set_Char_Size(face, 0, 48*64, hdpi, vdpi); // 16 points
	error = FT_Set_Pixel_Sizes(face, 0, 48);
	assert(!error);

	// create opengl texture
	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	settings.filter_min = GL_LINEAR;
	settings.filter_mag = GL_LINEAR;
	settings.internal_format = GL_ALPHA;
	texture_init(&atlas, 256, 256, &settings);
	glBindTexture(GL_TEXTURE_2D, atlas.texture);

	// render each character
	FT_ULong character = 'g';
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	{
		FT_UInt glyph_index = FT_Get_Char_Index(face, character);
		// 0 is "missing glyph" and renders as a box/question mark/space...
		assert(glyph_index != 0);

		error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
		assert(!error);

		error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		assert(!error);

		glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0, face->glyph->bitmap.width, face->glyph->bitmap.rows,
			settings.internal_format, GL_UNSIGNED_BYTE,
			face->glyph->bitmap.buffer);

		printf("char: %dx%d\n", face->glyph->bitmap.width, face->glyph->bitmap.rows);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glBindTexture(GL_TEXTURE_2D, 0);

}

void text_global_destroy(void) {
	if (library == NULL) {
		return;
	}

	FT_Done_Face(face);
	FT_Done_FreeType(library);
}

void text_render(void) {
}

texture_t *text_get_atlas(void) {
	return &atlas;
}

void text_get_charsize(int *w, int *h) {
	*w = face->glyph->bitmap.width;
	*h = face->glyph->bitmap.rows;
}

