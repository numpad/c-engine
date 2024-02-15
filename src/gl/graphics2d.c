#include "graphics2d.h"

#include <assert.h>
#include <cglm/cglm.h>
#include "engine.h"
#include "gl/texture.h"
#include "gl/vbuffer.h"
#include "gl/shader.h"

//
// vars
//

static shader_t g_primitive_shader = {0};
static vbuffer_t g_rect_vbuffer = {0};

static float g_vertices[] = {
	// Quad
	0.0f, 0.0f,  0.0f, 0.0f,
	1.0f, 0.0f,  1.0f, 0.0f,
	0.0f, 1.0f,  0.0f, 1.0f,
	0.0f, 1.0f,  0.0f, 1.0f,
	1.0f, 0.0f,  1.0f, 0.0f,
	1.0f, 1.0f,  1.0f, 1.0f,
};

//
// private funcs
//

static void update_model_matrix(primitive2d_t *);

//
// public api
//

void graphics2d_init_rect(primitive2d_t *pri, float x, float y, float w, float h) {
	// shader
	// TODO: init shader elsewhere
	if (g_primitive_shader.program == 0) {
		shader_init_from_dir(&g_primitive_shader, "res/shader/primitive2d");
		// TODO: cleanup
	}

	// vertices
	// TODO: init vertices elsewhere
	if (g_rect_vbuffer.attribs == NULL) {
		vbuffer_init(&g_rect_vbuffer);
		vbuffer_set_data(&g_rect_vbuffer, sizeof(g_vertices), g_vertices);
		vbuffer_set_attrib(&g_rect_vbuffer, &g_primitive_shader, "a_position", 2, GL_FLOAT, 4 * sizeof(float), 0);
		vbuffer_set_attrib(&g_rect_vbuffer, &g_primitive_shader, "a_texcoord", 2, GL_FLOAT, 4 * sizeof(float), (void *)(2 * sizeof(float)));
		// TODO: cleanup
	}

	// model matrix
	graphics2d_set_position(pri, x, y);
	graphics2d_set_scale(pri, w, h);
	graphics2d_set_angle(pri, 0.0f);

	// texture
	pri->texture = NULL;

	// color
	graphics2d_set_color_mult(pri, 1.0f, 1.0f, 1.0f, 1.0f);

	// uv
	pri->subrect.x = 0.0f;
	pri->subrect.y = 0.0f;
	pri->subrect.z = 1.0f;
	pri->subrect.w = 1.0f;
}

void graphics2d_set_position(primitive2d_t *pri, float x, float y) {
	pri->pos.x = x;
	pri->pos.y = y;
	pri->pos.z = 0.0f;
	update_model_matrix(pri);
}

void graphics2d_set_position_z(primitive2d_t *pri, float x, float y, float z) {
	pri->pos.x = x;
	pri->pos.y = y;
	pri->pos.z = z;
	update_model_matrix(pri);
}
void graphics2d_set_scale(primitive2d_t *pri, float x, float y) {
	pri->scale.x = x;
	pri->scale.y = y;
	pri->scale.z = 1.0f;
	update_model_matrix(pri);
}

void graphics2d_set_angle(primitive2d_t *pri, float radians) {
	pri->angle = radians;
	update_model_matrix(pri);
}

void graphics2d_set_texture(primitive2d_t *pri, texture_t *texture) {
	pri->texture = texture;

	pri->subrect.x = 0.0f;
	pri->subrect.y = 0.0f;
	pri->subrect.z = 1.0f;
	pri->subrect.w = 1.0f;
}

void graphics2d_set_texture_tile(primitive2d_t *pri, texture_t *texture, int tile_width, int tile_height, int tile_x, int tile_y) {
	assert(texture != NULL);
	pri->texture = texture;

	const float uv_w = (float)tile_width / texture->width;
	const float uv_h = (float)tile_height / texture->height;
	pri->subrect.x = uv_w * tile_x;
	pri->subrect.y = uv_h * tile_y;
	pri->subrect.z = uv_w;
	pri->subrect.w = uv_h;
}

void graphics2d_set_color_mult(primitive2d_t *pri, float r, float g, float b, float a) {
	assert(pri != NULL);

	pri->color_mult.r = r;
	pri->color_mult.g = g;
	pri->color_mult.b = b;
	pri->color_mult.a = a;
}

void graphics2d_set_color_add(primitive2d_t *pri, float r, float g, float b, float a) {
	assert(pri != NULL);

	pri->color_add.r = r;
	pri->color_add.g = g;
	pri->color_add.b = b;
	pri->color_add.a = a;
}

void graphics2d_draw_primitive2d(engine_t *engine, primitive2d_t *pri) {
	shader_use(&g_primitive_shader);
	shader_set_uniform_mat4(&g_primitive_shader, "u_projection", (float*)engine->u_projection);
	shader_set_uniform_mat4(&g_primitive_shader, "u_view", (float*)engine->u_view);
	shader_set_uniform_mat4(&g_primitive_shader, "u_model", (float*)pri->model);
	shader_set_uniform_texture(&g_primitive_shader, "u_texture", GL_TEXTURE0, pri->texture);
	shader_set_uniform_vec4(&g_primitive_shader, "u_color_mult", pri->color_mult.raw);
	shader_set_uniform_vec4(&g_primitive_shader, "u_color_add", pri->color_add.raw);
	//vec2 texelsize = {1.0f / pri->texture->width, 1.0f / pri->texture->height};
	//shader_set_uniform_vec2(&g_primitive_shader, "u_texture_texelsize", texelsize);
	shader_set_uniform_vec4(&g_primitive_shader, "u_subrect", pri->subrect.raw);

	vbuffer_draw(&g_rect_vbuffer, 6);
}

//
// private impls
//

static void update_model_matrix(primitive2d_t *pri) {
	vec3 pivot = {pri->scale.x * 0.5f, pri->scale.y * 0.5f, 0.0f};
	vec3 rot_axis = {0.0f, 0.0f, 1.0f};

	glm_mat4_identity(pri->model);
	glm_translate(pri->model, pri->pos.raw);
	glm_rotate_at(pri->model, pivot, pri->angle, rot_axis);
	glm_scale(pri->model, pri->scale.raw);
}

