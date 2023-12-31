#ifndef VBUFFER_H
#define VBUFFER_H

#include <stdlib.h>
#include <SDL_opengles2.h>
#include "gl/shader.h"

struct vbuffer_attrib_s;

typedef struct vbuffer_s vbuffer_t;
struct vbuffer_s {
    GLuint buffer;

	size_t n_attribs;
	struct vbuffer_attrib_s *attribs;
};

void vbuffer_init(struct vbuffer_s *vbo);
void vbuffer_destroy(struct vbuffer_s *vbo);

void vbuffer_set_data(struct vbuffer_s *vbo, size_t sizeof_vertices, float *vertices);
void vbuffer_set_attrib(struct vbuffer_s *vbo, shader_t *, const char *attribname, GLint size, GLenum type, GLint stride, GLvoid *offset);

void vbuffer_draw(struct vbuffer_s *vbo, size_t n_vertices);

#endif
