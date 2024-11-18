#include "text.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
#include <cglm/types.h>
#include <hb.h>
#include <hb-ft.h>
#include "gl/texture.h"


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

	return NULL;
}

void fontatlas_init(fontatlas_t *fa, FT_Library library) {
	assert(fa != NULL);
	assert(library != NULL);

	fa->library_ref = library;
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

	// TODO: Is this really required? Feels like a wasteful
	//       way to prevent oversampling artifacts.
	//       Can FreeType generate empty padding?
	texture_clear(&fa->texture_atlas);
}

void fontatlas_destroy(fontatlas_t *fa) {
	assert(fa != NULL);

	fa->library_ref = NULL;
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
	assert(fa->num_glyphs == 0); // TODO: update existing glyphs

	fa_append_face_memory(fa);
	unsigned int face_index = fa->num_faces - 1;

	FT_Error error;
	error = FT_New_Face(fa->library_ref, filename, 0, &(fa->faces[face_index]));
	assert(!error);

	// TODO: handle dpi correctly.
	//       GetDisplayDPI doesnt really work on linux for me, reporting wrong DPI.
	//       The docs say SDL3 might fix this?
	//       Set_Pixel_Sizes works, but isn't crisp on highdpi.

	//float ddpi, hdpi, vdpi;
	//assert(SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0);
	//printf("Diagonal DPI: %f, Horizontal DPI: %f, Vertical DPI: %f", ddpi, hdpi, vdpi);
	//error = FT_Set_Char_Size(fa->faces[face_index], 0, 48*64, hdpi, vdpi); // 16 points
	error = FT_Set_Pixel_Sizes(fa->faces[face_index], 0, size);
	assert(!error);

	return face_index;
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

	for (unsigned int face_index = 0; face_index < fa->num_faces; ++face_index) {
		FT_Face face = fa->faces[face_index];

		FT_UInt glyph_index = FT_Get_Char_Index(face, character);
		// 0 is "missing glyph" and renders as a box/question mark/space...
		if (glyph_index == 0) {
			printf("no glyph for %c(0x%lX) and face %d\n", (char)character, character, face_index);
			continue;
		}

		FT_Error error;
		error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL); // FT_LOAD_RENDER??
		assert(!error);

		error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		assert(!error);

		if (free_x + face->glyph->bitmap.width >= fa->texture_atlas.width) {
			free_x = 0;
			free_y += height_max + fa->atlas_padding;
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
		fg->bearing.x = face->glyph->bitmap_left;
		fg->bearing.y = face->glyph->bitmap_top;

		// store glyph bitmap
		glTexSubImage2D(GL_TEXTURE_2D, 0,
			fg->texture_rect.x, fg->texture_rect.y, fg->texture_rect.z, fg->texture_rect.w,
			GL_ALPHA, GL_UNSIGNED_BYTE,
			face->glyph->bitmap.buffer);

		free_x += face->glyph->bitmap.width + fa->atlas_padding;
		height_max = (fg->texture_rect.w > height_max) ? fg->texture_rect.w : height_max;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void fontatlas_add_ascii_glyphs(fontatlas_t *fa) {
	for (unsigned long c = 0x21; c < 0x7f; ++c) {
		fontatlas_add_glyph(fa, c);
	}
}

fontatlas_glyph_t *fontatlas_get_glyph(fontatlas_t *fa, unsigned long glyph, unsigned char face_index) {
	assert(fa != NULL);
	assert(face_index < fa->num_faces);

	return fa_find_glyph(fa, glyph, face_index);
}

void fontatlas_write(fontatlas_t *fa, pipeline_t *pipeline, unsigned int str_len, char *str) {
	assert(fa != NULL);
	assert(str != NULL);
	assert(str_len > 0);

	pipeline_reset(pipeline);

	unsigned char face_index = 0;

	// shaping
	hb_buffer_t *hb_buffer = hb_buffer_create();
	hb_font_t *hb_font = hb_ft_font_create_referenced(fa->faces[face_index]);
	// TODO: remove control characters from buffer...
	hb_buffer_add_utf8(hb_buffer, str, str_len, 0, str_len);
	hb_buffer_guess_segment_properties(hb_buffer);
	hb_shape(hb_font, hb_buffer, NULL, 0);
	unsigned int shaped_len = hb_buffer_get_length(hb_buffer);
	//hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(hb_buffer, NULL);
	hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(hb_buffer, NULL);

	// control state
	unsigned int last_character = 0;

	vec2s cursor = {0};
	drawcmd_t cmd = DRAWCMD_INIT;
	for (unsigned int i = 0; i < shaped_len; ++i) {
		unsigned int character = str[i]; // TODO: infos[i].codepoint?

		// handle ccontrol characters
		if (character == '$' && i < shaped_len - 1 && str[i + 1] != '$') {
			goto loop_end;
		}
		if (last_character == '$') {
			int is_printable = 0;
			switch (character) {
				// 0-9 : Colors
				case '0': // reset all
					glm_vec4_zero(cmd.color_add);
					face_index = 0;
					break;
				case '1': // red
					glm_vec4_zero(cmd.color_add);
					cmd.color_add[0] = 1.0f;
					break;
				case '2': // green
					glm_vec4_zero(cmd.color_add);
					cmd.color_add[1] = 1.0f;
					break;
				case '3': // blue
					glm_vec4_zero(cmd.color_add);
					cmd.color_add[2] = 1.0f;
					break;
				// B,I : Style
				case 'B':
					face_index = 1;
					break;
				case 'I':
					face_index = 2;
					break;

				default: is_printable = 1; break;
			};
			if (!is_printable) goto loop_end;
		}

		fontatlas_glyph_t *glyph_info = fontatlas_get_glyph(fa, character, face_index);

		// skip non-drawable characters
		// TODO: differentiate between non-drawable and just not rasterized (yet?)
		if (glyph_info) {
			cmd.position.x = cursor.x + positions[i].x_offset + glyph_info->bearing.x;
			cmd.position.y = cursor.y + positions[i].y_offset - glyph_info->bearing.y;
			cmd.size.x = glyph_info->texture_rect.z;
			cmd.size.y = glyph_info->texture_rect.w;
			drawcmd_set_texture_subrect(&cmd, pipeline->texture, glyph_info->texture_rect.x, glyph_info->texture_rect.y, glyph_info->texture_rect.z, glyph_info->texture_rect.w);
			pipeline_emit(pipeline, &cmd);
		}

		cursor.x += positions[i].x_advance / 64.0f;
		cursor.y += positions[i].y_advance / 64.0f;
loop_end:
		last_character = character;
	}

	hb_buffer_destroy(hb_buffer);
	hb_font_destroy(hb_font);
}

void fontatlas_writef(fontatlas_t *fa, pipeline_t *pipeline, char *fmt, ...) {
	va_list args;

	// determine length of string
	va_start(args, fmt);
	int output_len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	// build output string
	char output[output_len + 1];
	assert(output != NULL);

	va_start(args, fmt);
	vsnprintf(output, output_len + 1, fmt, args);
	va_end(args);

	fontatlas_write(fa, pipeline, output_len, output);
}

