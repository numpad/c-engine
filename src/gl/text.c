#include "text.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
#include <cglm/types.h>
#include <hb.h>
#include <hb-ft.h>
#include "engine.h"
#include "gl/texture.h"


// font atlas

static void fa_append_glyph_memory(fontatlas_t *fa) {
	++fa->num_glyphs;
	fa->glyphs = realloc(fa->glyphs, sizeof(fontatlas_glyph_t) * fa->num_glyphs);
}

static fontatlas_glyph_t *fa_find_glyph(fontatlas_t *fa, unsigned long glyph, unsigned char style) {
	// TODO: binary search for ascii/all chars?
	//       we read a lot more often than writing, so reordering
	//       should be better than linear search on every read.
	//       measure this once it becomes relevant...
	for (unsigned int i = 0; i < fa->num_glyphs; ++i) {
		fontatlas_glyph_t *g = &fa->glyphs[i];
		if (g->code == glyph && g->style == style) {
			return g;
		}
	}

	return NULL;
}

static void apply_styling(uint character, enum fontatlas_font_style *style, drawcmd_t *cmd, int *is_printable) {
	*is_printable = 0;
	switch (character) {
		// 0-9 : Colors
		case '0': // reset all
			glm_vec4_zero(cmd->color_add);
			*style = FONTATLAS_REGULAR;
			break;
		case '1': // red
			glm_vec4_zero(cmd->color_add);
			cmd->color_add[0] = 1.0f;
			break;
		case '2': // green
			glm_vec4_zero(cmd->color_add);
			cmd->color_add[1] = 1.0f;
			break;
		case '3': // blue
			glm_vec4_zero(cmd->color_add);
			cmd->color_add[2] = 1.0f;
			break;
			// B,I : Style
		case 'B':
			if (*style == FONTATLAS_ITALIC) {
				*style = FONTATLAS_BOLDITALIC;
			} else {
				*style = FONTATLAS_BOLD;
			}
			break;
		case 'I':
			if (*style == FONTATLAS_ITALIC) {
				*style = FONTATLAS_BOLDITALIC;
			} else {
				*style = FONTATLAS_ITALIC;
			}
			break;

		default: *is_printable = 1; break;
	};
}

static void cursor_newline(vec2s *cursor, long line_height) {
	cursor->x = 0.0f;
	cursor->y += line_height;
}

void fontatlas_init(fontatlas_t *fa, struct engine *engine) {
	assert(fa != NULL);
	assert(engine != NULL);
	assert(engine->freetype != NULL);

	fa->library_ref = engine->freetype;
	for (usize i = 0; i < FONTATLAS_FONT_STYLE_MAX; ++i) {
		fa->faces[i] = NULL;
	}
	fa->glyphs = NULL;
	fa->num_glyphs = 0;
	fa->atlas_padding = 2;
	fa->pixel_ratio = engine->window_pixel_ratio;

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
	for (unsigned int i = 0; i < FONTATLAS_FONT_STYLE_MAX; ++i) {
		FT_Done_Face(fa->faces[i]);
	}
	free(fa->glyphs);
	fa->glyphs = NULL;
	fa->num_glyphs = 0;

	texture_destroy(&fa->texture_atlas);
}

unsigned int fontatlas_add_face(fontatlas_t *fa, const char *filename, int size) {
	assert(fa != NULL);
	assert(filename != NULL);
	assert(fa->num_glyphs == 0); // TODO: update existing glyphs

	FT_Face face;

	FT_Error error;
	error = FT_New_Face(fa->library_ref, filename, 0, &face);
	assert(error == 0 && "Could not open or create face");

	// TODO: This already improves High-DPI handling, but is this enough?
	float hdpi = (96.0f * fa->pixel_ratio);
	float vdpi = (96.0f * fa->pixel_ratio);
	error = FT_Set_Char_Size(face, 0, size * 64.0f, hdpi, vdpi);
	assert(!error);

	usize style          = FONTATLAS_FONT_STYLE_MAX;
	const uint is_bold   = face->style_flags & FT_STYLE_FLAG_BOLD;
	const uint is_italic = face->style_flags & FT_STYLE_FLAG_ITALIC;
	if (is_bold && is_italic) {
		style = FONTATLAS_BOLDITALIC;
	} else if (is_bold) {
		style = FONTATLAS_BOLD;
	} else if (is_italic) {
		style = FONTATLAS_ITALIC;
	} else {
		style = FONTATLAS_REGULAR;
	}
	assert(fa->faces[style] == NULL && "Font face is already loaded, replacing isn't supported!");
	fa->faces[style] = face;

	return style;
}

void fontatlas_add_glyph(fontatlas_t *fa, unsigned long character) {
	assert(fa != NULL);
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glBindTexture(GL_TEXTURE_2D, fa->texture_atlas.texture);

	// TODO: rect packing instead of this
	static int free_x = 0;
	static int free_y = 0;
	static int height_max = 0;

	for (unsigned int style = 0; style < FONTATLAS_FONT_STYLE_MAX; ++style) {
		FT_Face face = fa->faces[style];
		if (face == NULL) continue;

		FT_UInt glyph_index = FT_Get_Char_Index(face, character);
		// 0 is "missing glyph" and renders as a box/question mark/space...
		if (glyph_index == 0) {
			printf("no glyph for %c(0x%lX) and face %d\n", (char)character, character, style);
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
		fg->style = style;
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

fontatlas_glyph_t *fontatlas_get_glyph(fontatlas_t *fa, unsigned long glyph, enum fontatlas_font_style style) {
	assert(fa != NULL);
	assert(fa->faces[style] != NULL);

	return fa_find_glyph(fa, glyph, style);
}

void fontatlas_write_ex(fontatlas_t *fa, pipeline_t *pipeline, enum fontatlas_write_config config, uint max_width, unsigned int str_len, char *str) {
	assert(fa != NULL);
	assert(str != NULL);
	assert(str_len > 0);

	if (max_width == FONTATLAS_UNLIMITED) {
		max_width = (uint)-1;
	}

	pipeline_reset(pipeline);

	const float pixel_ratio = fa->pixel_ratio;
	enum fontatlas_font_style style = FONTATLAS_REGULAR;

	FT_Face face = fa->faces[style];
	// TODO: Is this ok for High-DPI?
	long line_height = (face->size->metrics.height >> 6) / pixel_ratio;
	// TODO: Not all fonts define line_height, calculate it ourselves?
	//       line_height = ((face->ascender - face->descender) + face->height) >> 6;
	assert(line_height > 0);

	// shaping
	hb_buffer_t *hb_buffer = hb_buffer_create();
	hb_font_t *hb_font = hb_ft_font_create_referenced(face);
	// TODO: remove unprintable & control characters from buffer...
	hb_buffer_add_utf8(hb_buffer, str, str_len, 0, str_len);
	hb_buffer_guess_segment_properties(hb_buffer);
	// TODO: disables ligatures, but we want those :(
	hb_feature_t no_ligatures[] = {
		{HB_TAG('l', 'i', 'g', 'a'), 0, 0, (unsigned int)-1},
		{HB_TAG('c', 'l', 'i', 'g'), 0, 0, (unsigned int)-1}
	};
	hb_shape(hb_font, hb_buffer, no_ligatures, count_of(no_ligatures));
	unsigned int shaped_len = hb_buffer_get_length(hb_buffer);
	//hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(hb_buffer, NULL);
	hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(hb_buffer, NULL);

	// control state
	unsigned int last_character = 0;

	vec2s cursor = { .x=0, .y=line_height };
	drawcmd_t cmd = DRAWCMD_INIT;
	for (uint i = 0; i < shaped_len; ++i) {
		uint character = str[i]; // TODO: infos[i].codepoint?

		// handle ccontrol characters
		const int is_last_char = (i == shaped_len - 1);
		if (character == '$' && !is_last_char && str[i + 1] != '$') {
			goto next_iteration;
		} else if (last_character == '$') {
			int is_printable = 0;
			apply_styling(character, &style, &cmd, &is_printable);
			if (!is_printable) goto next_iteration;
		} else if (character == '\n') {
			cursor_newline(&cursor, line_height);
			goto next_iteration;
		}

		fontatlas_glyph_t *glyph_info = fontatlas_get_glyph(fa, character, style);

		// skip non-drawable characters
		// TODO: differentiate between non-drawable and just not rasterized (yet?)
		if (glyph_info) {
			cmd.position.x = cursor.x + (positions[i].x_offset + glyph_info->bearing.x) / pixel_ratio;
			cmd.position.y = cursor.y + (positions[i].y_offset - glyph_info->bearing.y) / pixel_ratio;
			cmd.size.x = glyph_info->texture_rect.z / pixel_ratio;
			cmd.size.y = glyph_info->texture_rect.w / pixel_ratio;
			if (cmd.position.x + cmd.size.x >= max_width) {
				cursor_newline(&cursor, line_height);
				cmd.position.x = cursor.x + (positions[i].x_offset + glyph_info->bearing.x) / pixel_ratio;
				cmd.position.y = cursor.y + (positions[i].y_offset - glyph_info->bearing.y) / pixel_ratio;
			}

			drawcmd_set_texture_subrect(&cmd, pipeline->texture, glyph_info->texture_rect.x, glyph_info->texture_rect.y, glyph_info->texture_rect.z, glyph_info->texture_rect.w);
			pipeline_emit(pipeline, &cmd);
		}

		cursor.x += (positions[i].x_advance / 64.0f) / pixel_ratio;
		cursor.y += (positions[i].y_advance / 64.0f) / pixel_ratio;
next_iteration:
		last_character = character;
	}

	hb_buffer_destroy(hb_buffer);
	hb_font_destroy(hb_font);
}

void fontatlas_writef_ex(fontatlas_t *fa, pipeline_t *pipeline, enum fontatlas_write_config config, uint max_width, char *fmt, ...) {
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

	fontatlas_write_ex(fa, pipeline, config, max_width, output_len, output);
}

