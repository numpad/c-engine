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


// pipeline

void drawcmd_set_texture_subrect(drawcmd_t *cmd, texture_t *tex, int x, int y, int width, int height) {
	cmd->texture_subrect[0] = (float)x      / tex->width;
	cmd->texture_subrect[1] = (float)y      / tex->height;
	cmd->texture_subrect[2] = (float)width  / tex->width;
	cmd->texture_subrect[3] = (float)height / tex->height;
}

void drawcmd_set_texture_subrect_tile(drawcmd_t *cmd, texture_t *tex, int tile_width, int tile_height, int tile_x, int tile_y) {
	const float uv_w = (float)tile_width / tex->width;
	const float uv_h = (float)tile_height / tex->height;
	cmd->texture_subrect[0] = uv_w * tile_x;
	cmd->texture_subrect[1] = uv_h * tile_y;
	cmd->texture_subrect[2] = uv_w;
	cmd->texture_subrect[3] = uv_h;
}

void pipeline_init(pipeline_t *pl, shader_t *shader, int commands_max) {
	// config
	pl->components_per_vertex = 3 + 2 + 4 + 4; // pos + texcoord + color_mult + color_add
	pl->vertices_per_primitive = 6;
	int size_per_primitive = pl->components_per_vertex * sizeof(GLfloat) * pl->vertices_per_primitive;

	// state
	pl->commands_count = 0;
	pl->commands_max = commands_max;
	pl->texture = NULL;
	pl->shader = shader;

	glGenBuffers(1, &pl->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, pl->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, pl->commands_max * size_per_primitive, NULL, GL_DYNAMIC_DRAW);

	// init attribs
	pl->a_pos = glGetAttribLocation(pl->shader->program, "a_pos");
	pl->a_texcoord = glGetAttribLocation(pl->shader->program, "a_texcoord");
	pl->a_color_mult = glGetAttribLocation(pl->shader->program, "a_color_mult");
	pl->a_color_add = glGetAttribLocation(pl->shader->program, "a_color_add");
}

void pipeline_destroy(pipeline_t *pl) {
	glDeleteBuffers(1, &pl->vertex_buffer);
	glDisableVertexAttribArray(pl->a_pos);
	glDisableVertexAttribArray(pl->a_texcoord);
	if (pl->a_color_mult != -1) glDisableVertexAttribArray(pl->a_color_mult);
	if (pl->a_color_add != -1) glDisableVertexAttribArray(pl->a_color_add);
}

void pipeline_reset(pipeline_t *pl) {
	pl->commands_count = 0;
}

void pipeline_emit(pipeline_t *pl, drawcmd_t *cmd) {
	assert(pl->commands_count < pl->commands_max);

	float px = cmd->position.x;
	float py = cmd->position.y;
	float pz = cmd->position.z;

	float w = cmd->size.x;
	float h = cmd->size.y;

	float srx = cmd->texture_subrect[0];
	float sry = cmd->texture_subrect[1];
	float srw = cmd->texture_subrect[2];
	float srh = cmd->texture_subrect[3];

	float angle_cos = cosf(cmd->angle);
	float angle_sin = sinf(cmd->angle);
	float cx = px + cmd->origin.x * w + cmd->origin.z;
	float cy = py + cmd->origin.y * h + cmd->origin.w;

	float colm[4] = { cmd->color_mult[0], cmd->color_mult[1], cmd->color_mult[2], cmd->color_mult[3] };
	float cola[4] = { cmd->color_add[0], cmd->color_add[1], cmd->color_add[2], cmd->color_add[3] };

	/*
	float primitive_vertices[] = {
		px + 0.0f * w, py + 0.0f * h, pz,  srx + srw * 0.0f, sry + srh * 0.0f, 1.0f, 1.0f,
		px + 1.0f * w, py + 0.0f * h, pz,  srx + srw * 1.0f, sry + srh * 0.0f, 1.0f, 1.0f,
		px + 0.0f * w, py + 1.0f * h, pz,  srx + srw * 0.0f, sry + srh * 1.0f, 1.0f, 1.0f,
		px + 0.0f * w, py + 1.0f * h, pz,  srx + srw * 0.0f, sry + srh * 1.0f, 1.0f, 1.0f,
		px + 1.0f * w, py + 0.0f * h, pz,  srx + srw * 1.0f, sry + srh * 0.0f, 1.0f, 1.0f,
		px + 1.0f * w, py + 1.0f * h, pz,  srx + srw * 1.0f, sry + srh * 1.0f, 1.0f, 1.0f,
	};
	*/

	float primitive_vertices[] = {
		// position | texcoord | color_mult | color_add
		angle_cos * (px + 0.0f * w - cx) - angle_sin * (py + 0.0f * h - cy) + cx, angle_sin * (px + 0.0f * w - cx) + angle_cos * (py + 0.0f * h - cy) + cy, pz,  srx + srw * 0.0f, sry + srh * 0.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		angle_cos * (px + 1.0f * w - cx) - angle_sin * (py + 0.0f * h - cy) + cx, angle_sin * (px + 1.0f * w - cx) + angle_cos * (py + 0.0f * h - cy) + cy, pz,  srx + srw * 1.0f, sry + srh * 0.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		angle_cos * (px + 0.0f * w - cx) - angle_sin * (py + 1.0f * h - cy) + cx, angle_sin * (px + 0.0f * w - cx) + angle_cos * (py + 1.0f * h - cy) + cy, pz,  srx + srw * 0.0f, sry + srh * 1.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		angle_cos * (px + 0.0f * w - cx) - angle_sin * (py + 1.0f * h - cy) + cx, angle_sin * (px + 0.0f * w - cx) + angle_cos * (py + 1.0f * h - cy) + cy, pz,  srx + srw * 0.0f, sry + srh * 1.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		angle_cos * (px + 1.0f * w - cx) - angle_sin * (py + 0.0f * h - cy) + cx, angle_sin * (px + 1.0f * w - cx) + angle_cos * (py + 0.0f * h - cy) + cy, pz,  srx + srw * 1.0f, sry + srh * 0.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		angle_cos * (px + 1.0f * w - cx) - angle_sin * (py + 1.0f * h - cy) + cx, angle_sin * (px + 1.0f * w - cx) + angle_cos * (py + 1.0f * h - cy) + cy, pz,  srx + srw * 1.0f, sry + srh * 1.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
	};

	// sanity check:
	int size_per_primitive = pl->components_per_vertex * sizeof(GLfloat) * pl->vertices_per_primitive;
	assert(sizeof(primitive_vertices) == size_per_primitive);

	glBindBuffer(GL_ARRAY_BUFFER, pl->vertex_buffer);
	// TODO: map buffer, or use subdata once in draw?
	glBufferSubData(GL_ARRAY_BUFFER, pl->commands_count * sizeof(primitive_vertices), sizeof(primitive_vertices), primitive_vertices);

	++pl->commands_count;
}

void pipeline_draw(pipeline_t *pl, engine_t *engine) {
	shader_use(pl->shader);

	shader_set_uniform_mat4(pl->shader, "u_projection", (float *)engine->u_projection);
	shader_set_uniform_mat4(pl->shader, "u_view",       (float *)engine->u_view);
	if (pl->texture != NULL) {
		shader_set_uniform_texture(pl->shader, "u_texture", GL_TEXTURE0, pl->texture);
	}

	glBindBuffer(GL_ARRAY_BUFFER, pl->vertex_buffer);

	glEnableVertexAttribArray(pl->a_pos);
	glVertexAttribPointer    (pl->a_pos, 3, GL_FLOAT, GL_FALSE, pl->components_per_vertex * sizeof       (GLfloat), (void*)0);
	glEnableVertexAttribArray(pl->a_texcoord);
	glVertexAttribPointer    (pl->a_texcoord, 2, GL_FLOAT, GL_FALSE, pl->components_per_vertex * sizeof  (GLfloat), (void*)(3 * sizeof(GLfloat)));
	if (pl->a_color_mult != -1) {
		glEnableVertexAttribArray(pl->a_color_mult);
		glVertexAttribPointer    (pl->a_color_mult, 4, GL_FLOAT, GL_FALSE, pl->components_per_vertex * sizeof(GLfloat), (void*)(5 * sizeof(GLfloat)));
	}
	if (pl->a_color_add != -1) {
		glEnableVertexAttribArray(pl->a_color_add);
		glVertexAttribPointer    (pl->a_color_add, 4, GL_FLOAT, GL_FALSE, pl->components_per_vertex * sizeof (GLfloat), (void*)(9 * sizeof(GLfloat)));
	}

	glDrawArrays(GL_TRIANGLES, 0, pl->vertices_per_primitive * pl->commands_count);
}

