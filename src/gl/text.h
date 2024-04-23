#ifndef GL_TEXT_H
#define GL_TEXT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include "gl/texture.h"
#include "gl/graphics2d.h"


// text experiments

void text_global_init(void);
void text_global_destroy(void);

// font atlas

typedef struct fontatlas_s fontatlas_t;
typedef struct fontatlas_glyph_s fontatlas_glyph_t;

struct fontatlas_glyph_s {
	unsigned long code;
	ivec4s texture_rect;
	unsigned char face_index;
};

struct fontatlas_s {
	FT_Face *faces;
	unsigned int num_faces;

	texture_t texture_atlas;
	int atlas_padding;

	struct fontatlas_glyph_s *glyphs;
	unsigned int num_glyphs;
};

// init/destroy
void fontatlas_init(fontatlas_t *);
void fontatlas_destroy(fontatlas_t *);
// faces
unsigned int fontatlas_add_face(fontatlas_t *, const char *filename, int size);
// glyphs
void fontatlas_add_glyphs(fontatlas_t *fa, unsigned int num_chars, unsigned long *chars);
void fontatlas_add_glyph(fontatlas_t *, unsigned long glyph);
// rendering
fontatlas_glyph_t *fontatlas_get_glyph(fontatlas_t *, unsigned long glyph, unsigned char face_index);
void fontatlas_write(fontatlas_t *, pipeline_t *, char *fmt);

#endif

