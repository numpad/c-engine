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

enum shader_kind {
	SHADER_KIND_UNKNOWN,
	SHADER_KIND_MODEL,
	SHADER_KIND_GBUFFER
};

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

	enum shader_kind kind;
	union {
		struct {
			GLint projection;
			GLint view;
			GLint model;
			GLint diffuse;
			GLint bone_transforms;
			GLint is_rigged;
			GLint normal_matrix;
			GLint highlight;
			GLint player_world_pos
		} model;
		struct {
			GLint albedo;
			GLint position;
			GLint normal;
			GLint lut_size;
			GLint color_lut;
			GLint z_near;
			GLint z_far;
			GLint time;
		} gbuffer;
	} uniforms;
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
void shader_set_kind     (shader_t *, enum shader_kind);

// use
void shader_use(shader_t *);

// lazy uniform setters
void shader_set_uniform_buffer (shader_t *, const char *uniform_block_name, struct shader_ubo *ubo);
void shader_set_uniform_texture(shader_t *, const char *uniform_name, GLenum texture_unit, texture_t *texture);
void shader_set_uniform_int    (shader_t *, const char *uniform_name, GLint value);
void shader_set_uniform_float  (shader_t *, const char *uniform_name, float);
void shader_set_uniform_vec2   (shader_t *, const char *uniform_name, float[2]);
void shader_set_uniform_vec3   (shader_t *, const char *uniform_name, float[3]);
void shader_set_uniform_vec4   (shader_t *, const char *uniform_name, float[4]);
void shader_set_uniform_mat3   (shader_t *, const char *uniform_name, float[9]);
void shader_set_uniform_mat4   (shader_t *, const char *uniform_name, float[16]);

// uniform setters
void shader_set_texture (shader_t *, GLint uniform_location, GLenum texture_unit, texture_t *texture);
void shader_set_int     (shader_t *, GLint uniform_location, GLint);
void shader_set_float   (shader_t *, GLint uniform_location, float);
void shader_set_vec3    (shader_t *, GLint uniform_location, float[3]);
void shader_set_vec4    (shader_t *, GLint uniform_location, float[4]);
void shader_set_mat3    (shader_t *, GLint uniform_location, float[9]);
void shader_set_mat4    (shader_t *, GLint uniform_location, float[16]);
void shader_set_mat4_ptr(shader_t *, GLint uniform_location, usize count, float *);

#endif

