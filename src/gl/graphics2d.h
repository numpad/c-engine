#ifndef GRAPHICS2D_H
#define GRAPHICS2D_H

#include <cglm/cglm.h>
#include <cglm/struct.h>

typedef struct engine_s engine_t;
typedef struct vbuffer_s vbuffer_t;
typedef struct texture_s texture_t;

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

#endif

