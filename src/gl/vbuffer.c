#include "vbuffer.h"

#include <stb_ds.h>
#include <assert.h>

struct vbuffer_attrib_s {
	GLint location;
	GLuint size;
	GLenum type;
	GLuint stride;
	GLvoid* offset;
};

void vbuffer_init(struct vbuffer_s *vbo) {
	glGenBuffers(1, &vbo->buffer);
	vbo->attribs = NULL;
	vbo->n_attribs = 0;
}

void vbuffer_destroy(struct vbuffer_s *vbo) {
	glDeleteBuffers(1, &vbo->buffer);
	stbds_arrfree(vbo->attribs);
}

void vbuffer_set_data(struct vbuffer_s *vbo, size_t sizeof_vertices, float *vertices) {
	glBindBuffer(GL_ARRAY_BUFFER, vbo->buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof_vertices, vertices, GL_STATIC_DRAW);
	// TODO: restore previously bound ARRAY_BUFFER, or dont even unset?
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void vbuffer_set_attrib(struct vbuffer_s *vbo, GLuint shader, const char *attribname, GLint size, GLenum type, GLint stride, GLvoid *offset) {
	glBindBuffer(GL_ARRAY_BUFFER, vbo->buffer);

	assert(shader != 0);
	glUseProgram(shader);
	
	assert(attribname != NULL);

	struct vbuffer_attrib_s attribdata;
	attribdata.location = glGetAttribLocation(shader, attribname);
	attribdata.size = size;
	attribdata.type = type;
	attribdata.stride = stride;
	attribdata.offset = offset;

	// TODO: check if attrib is already set
	vbo->n_attribs++;
	stbds_arrput(vbo->attribs, attribdata);

	// TODO: restore previously bound ARRAY_BUFFER, or dont even unset?
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void vbuffer_draw(struct vbuffer_s *vbo, size_t n_vertices) {
	glBindBuffer(GL_ARRAY_BUFFER, vbo->buffer);

	for (size_t i = 0; i < vbo->n_attribs; ++i) {
		const struct vbuffer_attrib_s *attrib = &vbo->attribs[i];
		glVertexAttribPointer(attrib->location, attrib->size, attrib->type, GL_FALSE, attrib->stride, attrib->offset);
		glEnableVertexAttribArray(attrib->location);
	}

	glDrawArrays(GL_TRIANGLES, 0, n_vertices);
	// TODO: restore previously bound ARRAY_BUFFER, or dont even unset?
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

