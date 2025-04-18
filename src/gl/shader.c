#include "shader.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <SDL.h>
#include "gl/opengles3.h"
#include "gl/texture.h"
#include "util/str.h"
#include "util/fs.h"
#include "util/util.h"

//
// private funtions
//

static int shader_stage_new(GLenum type, const char *path);
static int shader_program_new(int vertex_shader, int fragment_shader);

static GLint uniform_location(shader_t *shader, const char *uniform_name);

static inline void assert_shader_is_bound(shader_t *shader) {
#ifdef DEBUG
	// assume correct shader is in use
	GLint current_program;
	glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
	assert(current_program > 0);
	assert((GLuint)current_program == shader->program);
#endif
}

//
// public api
//

// UBOs - uniform buffer objects
void shader_ubo_init(struct shader_ubo *ubo, usize data_len, const void *data) {
	assert(ubo != NULL);
	assert(data != NULL && data_len > 0);

	// buffer
	GLuint uniform_buffer;
	glGenBuffers(1, &uniform_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
	glBufferData(GL_UNIFORM_BUFFER, data_len, data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	// binding point
	// TODO: Handle multiple binding points...
	GLuint binding_point = 0;
	glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, uniform_buffer);
	//glBindBufferRange(GL_UNIFORM_BUFFER, binding_point, uniform_buffer, 0, data_len);
	GL_CHECK_ERROR();

	ubo->buffer_size = data_len;
	ubo->buffer = uniform_buffer;
	ubo->binding_point = binding_point;
}

void shader_ubo_destroy(struct shader_ubo *ubo) {
	assert(ubo != NULL);
	glDeleteBuffers(1, &ubo->buffer);
	ubo->buffer = 0;
	ubo->buffer_size = 0;
	ubo->binding_point = 0;
}

void shader_ubo_update(struct shader_ubo *ubo, usize data_len, const void *data) {
	assert(ubo != NULL);
	assert(data != NULL && data_len > 0);
	// TODO: We only handle updating the whole block,
	//       fine for now as this would overcomplicate the API.
	//       Maybe add partial updates in the future, and
	//       resize the buffer when data_len increases.
	assert(data_len == ubo->buffer_size);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo->buffer);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, data_len, data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// init & destroy

void shader_init(shader_t *shader, const char *vert_path, const char *frag_path) {
	assert(shader != NULL);
	assert(vert_path != NULL);
	assert(frag_path != NULL);
	shader->source.vert_path = str_copy(vert_path);
	shader->source.frag_path = str_copy(frag_path);
	int vs = shader_stage_new(GL_VERTEX_SHADER, vert_path);
	int fs = shader_stage_new(GL_FRAGMENT_SHADER, frag_path);
	assert(vs >= 0);
	assert(fs >= 0);
	shader->program = shader_program_new(vs, fs);
	assert(shader->program >= 0);
	shader->kind = SHADER_KIND_UNKNOWN;
}

void shader_init_from_dir(shader_t *program, const char *dir_path) {
	const int len = strlen(dir_path) + 13; // Enough room to store "fragment.glsl" and "vertex.glsl".
	const char vert_path[len + 1];
	const char frag_path[len + 1];
	assert(0 == str_path_replace_filename(dir_path, "vertex.glsl", len + 1, vert_path));
	assert(0 == str_path_replace_filename(dir_path, "fragment.glsl", len + 1, frag_path));

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

	str_free(shader->source.vert_path);
	str_free(shader->source.frag_path);
	shader->source.vert_path = 0;
	shader->source.frag_path = 0;
}

void shader_reload_source(shader_t *shader) {
	char *vert_path = str_copy(shader->source.vert_path);
	char *frag_path = str_copy(shader->source.frag_path);
	enum shader_kind previous_kind = shader->kind;
	shader_destroy(shader);
	shader_init(shader, vert_path, frag_path);
	shader_use(shader);
	shader_set_kind(shader, previous_kind);
	shader_use(NULL);
	str_free(vert_path);
	str_free(frag_path);
}

void shader_set_kind(shader_t *shader, enum shader_kind kind) {
	assert_shader_is_bound(shader);
	assert(shader != NULL);
	assert(kind != SHADER_KIND_UNKNOWN && "Makes no sense to set the kind to SHADER_KIND_UNKNOWN...");
	assert(shader->kind == SHADER_KIND_UNKNOWN && "Shader already has a kind specified, but tried to set again!");

	shader->kind = kind;
	switch (shader->kind) {
		case SHADER_KIND_UNKNOWN:
			break;
		case SHADER_KIND_MODEL:
			shader->uniforms.model.model           = glGetUniformLocation(shader->program, "u_model");
			shader->uniforms.model.view            = glGetUniformLocation(shader->program, "u_view");
			shader->uniforms.model.projection      = glGetUniformLocation(shader->program, "u_projection");
			shader->uniforms.model.bone_transforms = glGetUniformLocation(shader->program, "u_bone_transforms");
			shader->uniforms.model.is_rigged       = glGetUniformLocation(shader->program, "u_is_rigged");
			shader->uniforms.model.diffuse         = glGetUniformLocation(shader->program, "u_diffuse");
			shader->uniforms.model.normal_matrix   = glGetUniformLocation(shader->program, "u_normalMatrix");
			shader->uniforms.model.highlight       = glGetUniformLocation(shader->program, "u_highlight");
			shader->uniforms.model.player_world_pos= glGetUniformLocation(shader->program, "u_player_world_pos");
			break;
		case SHADER_KIND_GBUFFER:
			shader->uniforms.gbuffer.albedo    = glGetUniformLocation(shader->program, "u_albedo");
			shader->uniforms.gbuffer.position  = glGetUniformLocation(shader->program, "u_position");
			shader->uniforms.gbuffer.normal    = glGetUniformLocation(shader->program, "u_normal");
			shader->uniforms.gbuffer.lut_size  = glGetUniformLocation(shader->program, "u_lut_size");
			shader->uniforms.gbuffer.color_lut = glGetUniformLocation(shader->program, "u_color_lut");
			shader->uniforms.gbuffer.z_near    = glGetUniformLocation(shader->program, "u_z_near");
			shader->uniforms.gbuffer.z_far     = glGetUniformLocation(shader->program, "u_z_far");
			shader->uniforms.gbuffer.time      = glGetUniformLocation(shader->program, "u_time");
			break;
	};
}

// use
void shader_use(shader_t *shader) {
	// TODO: assert that GL_CURRENT_PROGRAM == 0, or otherwise
	//       GL_CURRENT_PROGRAM != shader->program, to ensure
	//       previous cleanup and/or duplicate shader_use()s.
	glUseProgram(shader == NULL ? 0 : shader->program);
}

// uniform setters

void shader_set_uniform_buffer(shader_t *shader, const char *uniform_block_name, struct shader_ubo *ubo) {
	assert(shader != NULL);
	assert(uniform_block_name != NULL);
	assert(ubo != NULL);

	// TODO: This is a problem when called a few times on mobile.
	GLuint ubo_index = glGetUniformBlockIndex(shader->program, uniform_block_name);
	GL_CHECK_ERROR();
	glUniformBlockBinding(shader->program, ubo_index, ubo->binding_point);
}

void shader_set_uniform_texture(shader_t *shader, const char *uniform_name, GLenum texture_unit, texture_t *texture) {
	assert(texture_unit >= GL_TEXTURE0 && texture_unit < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	assert(texture != NULL);

	// only set shader/uniform assignment
	if (shader != NULL && uniform_name != NULL) {
		glUseProgram(shader->program);
		GLint u_location = uniform_location(shader, uniform_name);
		glUniform1i(u_location, texture_unit - GL_TEXTURE0);
	}

	glActiveTexture(texture_unit);
	glBindTexture(GL_TEXTURE_2D, texture->texture);
	
}

void shader_set_uniform_int(shader_t *shader, const char *uniform_name, GLint value) {
	assert(shader != NULL);
	assert(uniform_name != NULL);

	glUseProgram(shader->program);
	GLint u_location = uniform_location(shader, uniform_name);
	glUniform1i(u_location, value);
}

void shader_set_uniform_float(shader_t *shader, const char *uniform_name, float v) {
	assert(shader != NULL);
	assert(uniform_name != NULL);
	
	glUseProgram(shader->program);
	GLint u_location = uniform_location(shader, uniform_name);
	glUniform1f(u_location, v);
}

void shader_set_uniform_vec2(shader_t *shader, const char *uniform_name, float vec[2]) {
	assert(shader != NULL);
	assert(uniform_name != NULL);
	
	glUseProgram(shader->program);
	GLint u_location = uniform_location(shader, uniform_name);
	glUniform2fv(u_location, 1, vec);
}

void shader_set_uniform_vec3(shader_t *shader, const char *uniform_name, float vec[3]) {
	assert(shader != NULL);
	assert(uniform_name != NULL);
	
	glUseProgram(shader->program);
	GLint u_location = uniform_location(shader, uniform_name);
	glUniform3fv(u_location, 1, vec);
}

void shader_set_uniform_vec4(shader_t *shader, const char *uniform_name, float vec[4]) {
	assert(shader != NULL);
	assert(uniform_name != NULL);
	
	glUseProgram(shader->program);
	GLint u_location = uniform_location(shader, uniform_name);
	glUniform4fv(u_location, 1, vec);
}

void shader_set_uniform_mat3(shader_t *shader, const char *uniform_name, float matrix[9]) {
	assert(shader != NULL);
	assert(shader->program > 0);
	assert(uniform_name != NULL);

	glUseProgram(shader->program);
	GLint u_location = uniform_location(shader, uniform_name);
	glUniformMatrix3fv(u_location, 1, GL_FALSE, matrix);
}

void shader_set_uniform_mat4(shader_t *shader, const char *uniform_name, float matrix[16]) {
	assert(shader != NULL);
	assert(shader->program > 0);
	assert(uniform_name != NULL);

	glUseProgram(shader->program);
	GLint u_location = uniform_location(shader, uniform_name);
	glUniformMatrix4fv(u_location, 1, GL_FALSE, matrix);
}

// uniform setters

void shader_set_texture(shader_t *shader, GLint uniform_location, GLenum texture_unit, texture_t *texture) {
	assert_shader_is_bound(shader);
	assert(texture_unit >= GL_TEXTURE0 && texture_unit < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	assert(texture != NULL);

	// only set shader/uniform assignment
	glUniform1i(uniform_location, texture_unit - GL_TEXTURE0);

	glActiveTexture(texture_unit);
	glBindTexture(GL_TEXTURE_2D, texture->texture);
}

void shader_set_int(shader_t *shader, GLint uniform_location, GLint value) {
	assert_shader_is_bound(shader);
	glUniform1i(uniform_location, value);
}

void shader_set_float(shader_t *shader, GLint uniform_location, float value) {
	assert_shader_is_bound(shader);
	glUniform1f(uniform_location, value);
}

void shader_set_vec3(shader_t *shader, GLint uniform_location, float values[3]) {
	assert_shader_is_bound(shader);
	glUniform3f(uniform_location, values[0], values[1], values[2]);
}

void shader_set_vec4(shader_t *shader, GLint uniform_location, float values[4]) {
	assert_shader_is_bound(shader);
	glUniform4f(uniform_location, values[0], values[1], values[2], values[3]);
}

void shader_set_mat3(shader_t *shader, GLint uniform_location, float matrix[9]) {
	assert_shader_is_bound(shader);
	glUniformMatrix3fv(uniform_location, 1, GL_FALSE, matrix);
}

void shader_set_mat4(shader_t *shader, GLint uniform_location, float matrix[16]) {
	assert_shader_is_bound(shader);
	glUniformMatrix4fv(uniform_location, 1, GL_FALSE, matrix);
}

void shader_set_mat4_ptr(shader_t *shader, GLint uniform_location, usize count, float *matrix) {
	assert_shader_is_bound(shader);
	glUniformMatrix4fv(uniform_location, count, GL_FALSE, matrix);
}


//
// private impl
//

static int shader_stage_new(GLenum type, const char *path) {
	GLchar *shadersrc;
	long shadersrc_len;
	if (fs_readfile(path, &shadersrc, &shadersrc_len) != FS_OK) {
		fprintf(stderr, "error: failed reading shader \"%s\"...\n", path);
		return -1;
	}

	int shader = glCreateShader(type);
	const GLchar *shadersrc_const = shadersrc; // TODO: Cringe, but required for now.
	glShaderSource(shader, 1, &shadersrc_const, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		GLint log_len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
		
		char log[log_len];
		glGetShaderInfoLog(shader, log_len, NULL, log);

		fprintf(stderr, "error compiling shader \"%s\":\n%s\n", path, log);
		shader = -1;
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
		return -1;
	}

	return program;
}

static GLint uniform_location(shader_t *shader, const char *uniform_name) {
	assert_shader_is_bound(shader);
	GLint u_location = glGetUniformLocation(shader->program, uniform_name);

	return u_location;
}

