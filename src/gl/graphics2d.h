#ifndef GRAPHICS2D_H
#define GRAPHICS2D_H

#include <cglm/cglm.h>
#include <cglm/struct.h>
#include "gl/shader.h"

struct engine;

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
void drawcmd_flip_texture_subrect(drawcmd_t *, int flip_x, int flip_y);

typedef struct {
	// config
	int components_per_vertex;
	int vertices_per_primitive;
	int z_sorting_enabled;

	// state
	unsigned int vertex_buffer;
	int commands_count;
	int commands_max;
	texture_t *texture;
	shader_t *shader;

	drawcmd_t *cmd_buffer;

	// attribs
	struct {
		int a_pos;
		int a_texcoord;
		int a_color_mult;
		int a_color_add;
	} attribs;
} pipeline_t;

void pipeline_init(pipeline_t *, shader_t *, int commands_max);
void pipeline_destroy(pipeline_t *);

void pipeline_reset(pipeline_t *);
void pipeline_emit(pipeline_t *, drawcmd_t *);
void pipeline_set_transform(pipeline_t *, mat4 model);

void pipeline_draw(pipeline_t *, struct engine *);
void pipeline_draw_ortho(pipeline_t *, float w, float h);

#endif

