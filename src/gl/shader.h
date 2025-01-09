#ifndef SHADER_H
#define SHADER_H

#include "gl/opengles3.h"
#include "util/util.h"

//
// forward decls
//
typedef struct texture_s texture_t;

//
// typedefs, structs & enums
//

typedef struct shader shader_t;

struct shader_ubo {
	GLuint buffer;
	usize buffer_size;
	GLuint binding_point;
};

struct shader {
	GLuint program;
	struct {
		char *vert_path;
		char *frag_path;
	} source;
};

//
// public api
//

// global uniform
void shader_ubo_init   (struct shader_ubo *, usize data_len, const void *data);
void shader_ubo_destroy(struct shader_ubo *);
void shader_ubo_update (struct shader_ubo *, usize data_len, const void *data);

// init & destroy
void shader_init         (shader_t *, const char *vert_path, const char *frag_path);
void shader_init_from_dir(shader_t *, const char *dir_path);
void shader_destroy      (shader_t *);
void shader_reload_source(shader_t *);

// use
void shader_use(shader_t *);

// uniform setters
void shader_set_uniform_buffer (shader_t *, const char *uniform_block_name, struct shader_ubo *ubo);
void shader_set_uniform_texture(shader_t *, const char *uniform_name, GLenum texture_unit, texture_t *texture);
void shader_set_uniform_int    (shader_t *, const char *uniform_name, GLint value);
void shader_set_uniform_float  (shader_t *, const char *uniform_name, float);
void shader_set_uniform_vec2   (shader_t *, const char *uniform_name, float[2]);
void shader_set_uniform_vec3   (shader_t *, const char *uniform_name, float[3]);
void shader_set_uniform_vec4   (shader_t *, const char *uniform_name, float[4]);
void shader_set_uniform_mat3   (shader_t *, const char *uniform_name, float[9]);
void shader_set_uniform_mat4   (shader_t *, const char *uniform_name, float[16]);

#endif

