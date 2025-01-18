#ifndef GL_TEXT_H
#define GL_TEXT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include <cglm/cglm.h>
#include "gl/texture.h"
#include "gl/graphics2d.h"

#define FONTATLAS_UNLIMITED 0

// text experiments

void text_global_init(void);
void text_global_destroy(void);

// font atlas

typedef struct fontatlas_s fontatlas_t;
typedef struct fontatlas_glyph_s fontatlas_glyph_t;

enum fontatlas_font_style {
	FONTATLAS_REGULAR,
	FONTATLAS_BOLD,
	FONTATLAS_ITALIC,
	FONTATLAS_BOLDITALIC,
	FONTATLAS_FONT_STYLE_MAX
};

enum fontatlas_write_config {
	FONTATLAS_WRITE_DEFAULT = 0,
	// horizontal
	FONTATLAS_ALIGN_LEFT    = 1 << 0,
	FONTATLAS_ALIGN_RIGHT   = 1 << 1,
	FONTATLAS_ALIGN_CENTER  = 1 << 2,
	// vertical
	FONTATLAS_ALIGN_TOP     = 1 << 3,
	FONTATLAS_ALIGN_BOTTOM  = 1 << 4,
	FONTATLAS_ALIGN_MIDDLE  = 1 << 5
};

struct fontatlas_glyph_s {
	unsigned long code;
	ivec4s texture_rect;
	enum fontatlas_font_style style;
	vec2s bearing;
};

struct fontatlas_s {
	FT_Library library_ref;
	FT_Face faces[FONTATLAS_FONT_STYLE_MAX];

	texture_t texture_atlas;
	int atlas_padding;

	struct fontatlas_glyph_s *glyphs;
	unsigned int num_glyphs;

	// From engine->window_pixel_ratio
	float pixel_ratio;
};

// init/destroy
void fontatlas_init(fontatlas_t *, struct engine *engine);
void fontatlas_destroy(fontatlas_t *);
// faces
unsigned int fontatlas_add_face(fontatlas_t *, const char *filename, int size);
// glyphs
void fontatlas_add_glyph(fontatlas_t *, ulong glyph);
void fontatlas_add_ascii_glyphs(fontatlas_t *fa);
// rendering
fontatlas_glyph_t *fontatlas_get_glyph(fontatlas_t *, unsigned long glyph, enum fontatlas_font_style face_index);
void fontatlas_write_ex(fontatlas_t *, pipeline_t *, enum fontatlas_write_config, uint max_width, unsigned int str_len, char *str);
void fontatlas_writef_ex(fontatlas_t *, pipeline_t *, enum fontatlas_write_config, uint max_width, char *fmt, ...);

#endif

