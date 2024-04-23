#include "text.h"

#include <stdio.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
#include <cglm/types.h>
#include "gl/texture.h"
#include "util/util.h"


static FT_Library library;

void text_global_init(void) {
	assert(library == NULL);

	FT_Error error;

	error = FT_Init_FreeType(&library);
	if (error) {
		printf("error initializing freetype!\n");
		return;
	}
}

void text_global_destroy(void) {
	if (library == NULL) {
		return;
	}

	FT_Done_FreeType(library);
}


// font atlas

static void fa_append_face_memory(fontatlas_t *fa) {
	++fa->num_faces;
	fa->faces = realloc(fa->faces, sizeof(FT_Face) * fa->num_faces);
}

static void fa_append_glyph_memory(fontatlas_t *fa) {
	++fa->num_glyphs;
	fa->glyphs = realloc(fa->glyphs, sizeof(fontatlas_glyph_t) * fa->num_glyphs);
}

static fontatlas_glyph_t *fa_find_glyph(fontatlas_t *fa, unsigned long glyph, unsigned char face_index) {
	for (unsigned int i = 0; i < fa->num_glyphs; ++i) {
		fontatlas_glyph_t *g = &fa->glyphs[i];
		if (g->code == glyph && g->face_index == face_index) {
			return g;
		}
	}

	printf("couldn't find %lu with face %d\n", glyph, face_index);
	return NULL;
}

void fontatlas_init(fontatlas_t *fa) {
	assert(fa != NULL);

	fa->faces = NULL;
	fa->num_faces = 0;
	fa->glyphs = NULL;
	fa->num_glyphs = 0;
	fa->atlas_padding = 2;

	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	settings.filter_min = GL_LINEAR;
	settings.filter_mag = GL_LINEAR;
	settings.internal_format = GL_ALPHA;
	texture_init(&fa->texture_atlas, 2048, 2048, &settings);
}

void fontatlas_destroy(fontatlas_t *fa) {
	assert(fa != NULL);

	for (unsigned int i = 0; i < fa->num_faces; ++i) {
		FT_Done_Face(fa->faces[i]);
	}
	free(fa->faces);
	fa->faces = NULL;
	fa->num_faces = 0;
	free(fa->glyphs);
	fa->glyphs = NULL;
	fa->num_glyphs = 0;

	texture_destroy(&fa->texture_atlas);
}

unsigned int fontatlas_add_face(fontatlas_t *fa, const char *filename, int size) {
	assert(fa != NULL);
	assert(filename != NULL);
	assert(library != NULL);
	assert(fa->num_glyphs == 0); // TODO: update existing glyphs

	fa_append_face_memory(fa);
	unsigned int face_index = fa->num_faces - 1;

	FT_Error error;
	error = FT_New_Face(library, filename, 0, &(fa->faces[face_index]));
	assert(!error);

	// TODO: handle dpi correctly.
	//       GetDisplayDPI doesnt really work on linux for me, reporting wrong DPI.
	//       The docs say SDL3 might fix this?
	//       Set_Pixel_Sizes works, but isn't crisp on highdpi.

	//float ddpi, hdpi, vdpi;
	//assert(SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0);
	//printf("Diagonal DPI: %f, Horizontal DPI: %f, Vertical DPI: %f", ddpi, hdpi, vdpi);
	//error = FT_Set_Char_Size(face, 0, 48*64, hdpi, vdpi); // 16 points
	error = FT_Set_Pixel_Sizes(fa->faces[face_index], 0, size);
	assert(!error);

	return face_index;
}

void fontatlas_add_glyphs(fontatlas_t *fa, unsigned int num_chars, unsigned long *chars) {
	assert(fa != NULL);
	assert(num_chars > 0);

	for (unsigned int i = 0; i < num_chars; ++i) {
		fontatlas_add_glyph(fa, chars[i]);
	}
}

void fontatlas_add_glyph(fontatlas_t *fa, unsigned long character) {
	assert(fa != NULL);
	assert(fa->num_faces > 0); // TODO: doesn't make sense as we wouldnt create any glyphs
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glBindTexture(GL_TEXTURE_2D, fa->texture_atlas.texture);

	// TODO: rect packing instead of this
	static int free_x = 0;
	static int free_y = 0;
	static int height_max = 0;
	const int padding = 2;

	for (unsigned int face_index = 0; face_index < fa->num_faces; ++face_index) {
		FT_Face face = fa->faces[face_index];

		FT_UInt glyph_index = FT_Get_Char_Index(face, character);
		// 0 is "missing glyph" and renders as a box/question mark/space...
		if (glyph_index == 0) {
			printf("no glyph for %c(0x%lX) and face %d\n", (char)character, character, face_index);
			continue;
		}
		assert(glyph_index != 0);

		FT_Error error;
		error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL);
		assert(!error);

		error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		assert(!error);

		if (free_x + face->glyph->bitmap.width >= fa->texture_atlas.width) {
			free_x = 0;
			free_y += height_max + padding;
			height_max = 0;
		}

		fa_append_glyph_memory(fa);
		fontatlas_glyph_t *fg = &fa->glyphs[fa->num_glyphs - 1];
		fg->code = character;
		fg->texture_rect.x = free_x;
		fg->texture_rect.y = free_y;
		fg->texture_rect.z = face->glyph->bitmap.width;
		fg->texture_rect.w = face->glyph->bitmap.rows;
		fg->face_index = face_index;

		// store glyph bitmap
		glTexSubImage2D(GL_TEXTURE_2D, 0,
			fg->texture_rect.x, fg->texture_rect.y, fg->texture_rect.z, fg->texture_rect.w,
			GL_ALPHA, GL_UNSIGNED_BYTE,
			face->glyph->bitmap.buffer);

		free_x += face->glyph->bitmap.width + padding;
		height_max = (fg->texture_rect.w > height_max) ? fg->texture_rect.w : height_max;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glBindTexture(GL_TEXTURE_2D, 0);
}

fontatlas_glyph_t *fontatlas_get_glyph(fontatlas_t *fa, unsigned long glyph, unsigned char face_index) {
	assert(fa != NULL);
	assert(face_index < fa->num_faces);

	return fa_find_glyph(fa, glyph, face_index);
}

void fontatlas_write(fontatlas_t *fa, pipeline_t *pipeline, char *fmt) {
	assert(fa != NULL);
	assert(fmt != NULL);

	unsigned char face_index = 0;

	const uint fmt_len = strlen(fmt);
	float x = 0.0f;

	for (unsigned int i = 0; i < fmt_len; ++i) {
		unsigned int character = fmt[i];
		// TODO: implement control characters
		if (character == '*') {
			face_index = 1;
			continue;
		}
		fontatlas_glyph_t *glyph_info = fontatlas_get_glyph(fa, character, face_index);
		assert(glyph_info != NULL); // TODO: allow this, for spaces etc. maybe unicode placeholder?

		drawcmd_t cmd = DRAWCMD_INIT;
		cmd.position.x = 200.0f + x;
		cmd.position.y = 150.0f;
		cmd.size.x = glyph_info->texture_rect.z;
		cmd.size.y = glyph_info->texture_rect.w;
		drawcmd_set_texture_subrect(&cmd, pipeline->texture, glyph_info->texture_rect.x, glyph_info->texture_rect.y, glyph_info->texture_rect.z, glyph_info->texture_rect.w);

		x += glyph_info->texture_rect.z;

		pipeline_emit(pipeline, &cmd);
	}
}

