#include "graphics2d.h"

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
		float g_vertices[] = {
			// Quad
			0.0f, 0.0f,  0.0f, 0.0f,
			1.0f, 0.0f,  1.0f, 0.0f,
			0.0f, 1.0f,  0.0f, 1.0f,
			0.0f, 1.0f,  0.0f, 1.0f,
			1.0f, 0.0f,  1.0f, 0.0f,
			1.0f, 1.0f,  1.0f, 1.0f,
		};
		vbuffer_init(&g_rect_vbuffer);
		vbuffer_set_data(&g_rect_vbuffer, sizeof(g_vertices), g_vertices);
		vbuffer_set_attrib(&g_rect_vbuffer, &g_primitive_shader, "a_position", 2, GL_FLOAT, 4 * sizeof(float), 0);
		vbuffer_set_attrib(&g_rect_vbuffer, &g_primitive_shader, "a_texcoord", 2, GL_FLOAT, 4 * sizeof(float), (void *)(2 * sizeof(float)));
		// TODO: cleanup
	}

	// model matrix
	vec3 p = {x, y, 0.0f};
	vec3 s = {w, h, 1.0f};
	glm_mat4_identity(pri->model);
	glm_translate(pri->model, p);
	glm_scale(pri->model, s);

	// texture
	pri->texture = NULL;

	// uv
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

void graphics2d_draw_primitive2d(engine_t *engine, primitive2d_t *pri) {
	shader_use(&g_primitive_shader);
	shader_set_uniform_mat4(&g_primitive_shader, "u_projection", (float*)engine->u_projection);
	shader_set_uniform_mat4(&g_primitive_shader, "u_view", (float*)engine->u_view);
	shader_set_uniform_mat4(&g_primitive_shader, "u_model", (float*)pri->model);
	shader_set_uniform_texture(&g_primitive_shader, "u_texture", GL_TEXTURE0, pri->texture);
	shader_set_uniform_vec4(&g_primitive_shader, "u_subrect", pri->subrect.raw);

	vbuffer_draw(&g_rect_vbuffer, 6);
}

