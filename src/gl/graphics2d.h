#ifndef GRAPHICS2D_H
#define GRAPHICS2D_H

#include <cglm/cglm.h>
#include <cglm/struct.h>
#include "gl/shader.h"

typedef struct engine_s engine_t;
typedef struct vbuffer_s vbuffer_t;

typedef struct primitive2d_s primitive2d_t;
struct primitive2d_s {
	vbuffer_t *vbuffer;
	texture_t *texture;
	vec4s subrect;
	vec4s color_mult;
	vec4s color_add;

	vec3s pos;
	vec3s scale;
	float angle;
	mat4 model;
};

// init
void graphics2d_init_rect(primitive2d_t *, float x, float y, float w, float h);

// transform
void graphics2d_set_position(primitive2d_t *, float x, float y);
void graphics2d_set_position_z(primitive2d_t *, float x, float y, float z);
void graphics2d_set_scale(primitive2d_t *, float x, float y);
void graphics2d_set_angle(primitive2d_t *, float radians);

// texture
void graphics2d_set_texture(primitive2d_t *, texture_t *);
void graphics2d_set_texture_tile(primitive2d_t *, texture_t *, int tile_width, int tile_height, int tile_x, int tile_y);

// color
void graphics2d_set_color_mult(primitive2d_t *, float r, float g, float b, float a);
void graphics2d_set_color_add(primitive2d_t *, float r, float g, float b, float a);

// draw
void graphics2d_draw_primitive2d(engine_t *, primitive2d_t *);


// pipeline

#define DRAWCMD_INIT \
(drawcmd_t) {                                    \
	.position.raw    = {0.0f, 0.0f, 0.0f},       \
	.texture_subrect = {0.0f, 0.0f, 1.0f, 1.0f}, \
	.size.raw        = {1.0f, 1.0f},             \
	.angle           = 0.0f,                     \
	.origin.raw      = {0.5f, 0.5f, 0.0f, 0.0f}, \
	.color_mult      = {1.0f, 1.0f, 1.0f, 1.0f}, \
	.color_add       = {0.0f, 0.0f, 0.0f, 0.0f}, \
}

typedef struct {
	vec3s position;       // screen coordinates
	vec4 texture_subrect; // use with drawcmd_set_texture_subrect*()
	vec2s size;           // size
	float angle;          // rotation angle (radians)
	vec4s origin;         // rotation origin (x&y is percentage of size, z&w is offset)
	vec4 color_mult;
	vec4 color_add;
} drawcmd_t;

void drawcmd_set_texture_subrect(drawcmd_t *, texture_t *, int x, int y, int width, int height);
void drawcmd_set_texture_subrect_tile(drawcmd_t *, texture_t *, int tile_width, int tile_height, int tile_x, int tile_y);

typedef struct {
	// config
	int components_per_vertex;
	int vertices_per_primitive;

	// state
	unsigned int vertex_buffer;
	int commands_count;
	int commands_max;
	texture_t *texture;
	shader_t *shader;

	// attribs
	int a_pos;
	int a_texcoord;
	int a_color_mult;
	int a_color_add;
} pipeline_t;

void pipeline_init(pipeline_t *, shader_t *, int commands_max);
void pipeline_destroy(pipeline_t *);

void pipeline_reset(pipeline_t *);
void pipeline_emit(pipeline_t *, drawcmd_t *);

void pipeline_draw(pipeline_t *, engine_t *);

#endif

