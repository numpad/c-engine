#include "shader.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <SDL.h>
#include <SDL_opengles2.h>
#include "gl/texture.h"
#include "util/fs.h"

//
// private funtions
//

static int shader_stage_new(GLenum type, const char *path);
static int shader_program_new(int vertex_shader, int fragment_shader);

//
// public api
//

// init & destroy

void shader_init(shader_t *shader, const char *vert_path, const char *frag_path) {
	int vs = shader_stage_new(GL_VERTEX_SHADER, vert_path);
	int fs = shader_stage_new(GL_FRAGMENT_SHADER, frag_path);
	
	shader->program = shader_program_new(vs, fs);
}

void shader_init_from_dir(shader_t *program, const char *dir_path) {
	const size_t vert_len = snprintf(NULL, 0, "%s/vertex.glsl", dir_path);
	const size_t frag_len = snprintf(NULL, 0, "%s/fragment.glsl", dir_path);

	char vert_path[vert_len + 1];
	char frag_path[frag_len + 1];

	snprintf(vert_path, vert_len + 1, "%s/vertex.glsl", dir_path);
	snprintf(frag_path, frag_len + 1, "%s/fragment.glsl", dir_path);

	shader_init(program, vert_path, frag_path);
}

void shader_destroy(shader_t *shader) {
	GLsizei count;
	GLuint shaders[4];
	glGetAttachedShaders(shader->program, 4, &count, shaders);

	for (int i = 0; i < count; ++i) {
		glDeleteShader(shaders[i]);
	}

	glDeleteProgram(shader->program);
	shader->program = 0;
}

// uniform setters

void shader_set_uniform_texture(shader_t *shader, const char *uniform_name, GLenum texture_unit, texture_t *texture) {
	assert(texture_unit >= GL_TEXTURE0 && texture_unit < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	assert(texture != NULL);

	// only set shader/uniform assignment
	if (shader != NULL && uniform_name != NULL) {
		glUseProgram(shader->program);
		GLint u_location = glGetUniformLocation(shader->program, uniform_name);
		assert(u_location >= 0);
		glUniform1i(u_location, texture_unit - GL_TEXTURE0);
	}

	glActiveTexture(texture_unit);
	glBindTexture(GL_TEXTURE_2D, texture->texture);
	
}

void shader_set_uniform_vec4(shader_t *shader, const char *uniform_name, float vec[4]) {
	assert(shader != NULL);
	assert(uniform_name != NULL);
	
	glUseProgram(shader->program);
	GLint u_location = glGetUniformLocation(shader->program, uniform_name);
	assert(u_location >= 0);
	glUniform4fv(u_location, 1, vec);
}

void shader_set_uniform_mat4(shader_t *shader, const char *uniform_name, float matrix[16]) {
	assert(shader != NULL);
	assert(shader->program > 0);
	assert(uniform_name != NULL);

	glUseProgram(shader->program);
	GLint u_location = glGetUniformLocation(shader->program, uniform_name);
	assert(u_location >= 0);
	glUniformMatrix4fv(u_location, 1, GL_FALSE, matrix);
}

//
// private impl
//

static int shader_stage_new(GLenum type, const char *path) {
	GLchar *shadersrc;
	long shadersrc_len;
	if (fs_readfile(path, &shadersrc, &shadersrc_len) != FS_OK) {
		return -1;
	}

	int shader = glCreateShader(type);
	glShaderSource(shader, 1, &shadersrc, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		GLint log_len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
		
		char log[log_len];
		glGetShaderInfoLog(shader, log_len, NULL, log);

		fprintf(stderr, "error compiling shader \"%s\":\n%s\n", path, log);
	}

	free(shadersrc);

	return shader;
}

static int shader_program_new(int vertex_shader, int fragment_shader) {
	int program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		GLint log_len;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);

		char log[log_len];
		glGetProgramInfoLog(program, log_len, NULL, log);

		fprintf(stderr, "error linking shader program:\n%s\n", log);
	}

	return program;
}

