#include "shader.h"

#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_opengles2.h>
#include "util/fs.h"

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

int shader_new(const char *vert_path, const char *frag_path) {
	int vs = shader_stage_new(GL_VERTEX_SHADER, vert_path);
	int fs = shader_stage_new(GL_FRAGMENT_SHADER, frag_path);
	
	int program = shader_program_new(vs, fs);
	return program;
}

void shader_delete(int program) {
	GLsizei count;
	GLuint shaders[4];
	glGetAttachedShaders(program, 4, &count, shaders);

	for (int i = 0; i < count; ++i) {
		glDeleteShader(shaders[i]);
	}

	glDeleteProgram(program);
}

