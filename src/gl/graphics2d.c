#include "graphics2d.h"

#include <assert.h>
#include <cglm/cglm.h>
#include "engine.h"
#include "gl/texture.h"
#include "gl/vbuffer.h"
#include "gl/shader.h"
#include <SDL_opengles2.h>

// calculate vertices for a draw command and write them into a buffer.
// returns the number of bytes written.
static unsigned int write_drawcmd_vertices(drawcmd_t *cmd, float *vertices) {
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

	// 0--1
	// | /|
	// |/ |
	// 2--3

	/*
	float primitive_vertices[] = {
		2: px + 0.0f * w, py + 1.0f * h, pz,  srx + srw * 0.0f, sry + srh * 1.0f, 1.0f, 1.0f,
		1: px + 1.0f * w, py + 0.0f * h, pz,  srx + srw * 1.0f, sry + srh * 0.0f, 1.0f, 1.0f,
		0: px + 0.0f * w, py + 0.0f * h, pz,  srx + srw * 0.0f, sry + srh * 0.0f, 1.0f, 1.0f,
		2: px + 0.0f * w, py + 1.0f * h, pz,  srx + srw * 0.0f, sry + srh * 1.0f, 1.0f, 1.0f,
		3: px + 1.0f * w, py + 1.0f * h, pz,  srx + srw * 1.0f, sry + srh * 1.0f, 1.0f, 1.0f,
		1: px + 1.0f * w, py + 0.0f * h, pz,  srx + srw * 1.0f, sry + srh * 0.0f, 1.0f, 1.0f,
	};
	*/

	float primitive_vertices[] = {
		// position | texcoord | color_mult | color_add
		/*2*/ angle_cos * (px + 0.0f * w - cx) - angle_sin * (py + 1.0f * h - cy) + cx, angle_sin * (px + 0.0f * w - cx) + angle_cos * (py + 1.0f * h - cy) + cy, pz,  srx + srw * 0.0f, sry + srh * 1.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		/*1*/ angle_cos * (px + 1.0f * w - cx) - angle_sin * (py + 0.0f * h - cy) + cx, angle_sin * (px + 1.0f * w - cx) + angle_cos * (py + 0.0f * h - cy) + cy, pz,  srx + srw * 1.0f, sry + srh * 0.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		/*0*/ angle_cos * (px + 0.0f * w - cx) - angle_sin * (py + 0.0f * h - cy) + cx, angle_sin * (px + 0.0f * w - cx) + angle_cos * (py + 0.0f * h - cy) + cy, pz,  srx + srw * 0.0f, sry + srh * 0.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		/*2*/ angle_cos * (px + 0.0f * w - cx) - angle_sin * (py + 1.0f * h - cy) + cx, angle_sin * (px + 0.0f * w - cx) + angle_cos * (py + 1.0f * h - cy) + cy, pz,  srx + srw * 0.0f, sry + srh * 1.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		/*3*/ angle_cos * (px + 1.0f * w - cx) - angle_sin * (py + 1.0f * h - cy) + cx, angle_sin * (px + 1.0f * w - cx) + angle_cos * (py + 1.0f * h - cy) + cy, pz,  srx + srw * 1.0f, sry + srh * 1.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
		/*1*/ angle_cos * (px + 1.0f * w - cx) - angle_sin * (py + 0.0f * h - cy) + cx, angle_sin * (px + 1.0f * w - cx) + angle_cos * (py + 0.0f * h - cy) + cy, pz,  srx + srw * 1.0f, sry + srh * 0.0f,  colm[0], colm[1], colm[2], colm[3],  cola[0], cola[1], cola[2], cola[3],
	};

	memcpy(vertices, primitive_vertices, sizeof(primitive_vertices));
	return sizeof(primitive_vertices);
}

static void draw_pipeline(pipeline_t *pl, mat4 u_projection, mat4 u_view) {
	shader_use(pl->shader);

	shader_set_uniform_mat4(pl->shader, "u_projection", (float *)u_projection);
	shader_set_uniform_mat4(pl->shader, "u_view",       (float *)u_view);
	if (pl->texture != NULL) {
		shader_set_uniform_texture(pl->shader, "u_texture", GL_TEXTURE0, pl->texture);
	}

	glBindBuffer(GL_ARRAY_BUFFER, pl->vertex_buffer);
	
	// setup vertex data
	// TODO: upload buffer once (or mmap?) instead of many single subdata calls
	const float sizeof_each_primitive = pl->components_per_vertex * pl->vertices_per_primitive * sizeof(GLfloat);
	static float primitive_vertices[128];
	for (int i = 0; i < pl->commands_count; ++i) {
		unsigned int sizeof_primitive_vertices = write_drawcmd_vertices(&pl->cmd_buffer[i], primitive_vertices);
		glBufferSubData(GL_ARRAY_BUFFER, i * sizeof_each_primitive, sizeof_primitive_vertices, primitive_vertices);
	}

	// setup attribs
	glEnableVertexAttribArray(pl->attribs.a_pos);
	glVertexAttribPointer    (pl->attribs.a_pos, 3, GL_FLOAT, GL_FALSE, pl->components_per_vertex * sizeof       (GLfloat), (void*)0);
	glEnableVertexAttribArray(pl->attribs.a_texcoord);
	glVertexAttribPointer    (pl->attribs.a_texcoord, 2, GL_FLOAT, GL_FALSE, pl->components_per_vertex * sizeof  (GLfloat), (void*)(3 * sizeof(GLfloat)));
	if (pl->attribs.a_color_mult != -1) {
		glEnableVertexAttribArray(pl->attribs.a_color_mult);
		glVertexAttribPointer    (pl->attribs.a_color_mult, 4, GL_FLOAT, GL_FALSE, pl->components_per_vertex * sizeof(GLfloat), (void*)(5 * sizeof(GLfloat)));
	}
	if (pl->attribs.a_color_add != -1) {
		glEnableVertexAttribArray(pl->attribs.a_color_add);
		glVertexAttribPointer    (pl->attribs.a_color_add, 4, GL_FLOAT, GL_FALSE, pl->components_per_vertex * sizeof (GLfloat), (void*)(9 * sizeof(GLfloat)));
	}

	// draw
	glDrawArrays(GL_TRIANGLES, 0, pl->vertices_per_primitive * pl->commands_count);
}


// pipeline

void drawcmd_set_texture_subrect(drawcmd_t *cmd, texture_t *tex, int x, int y, int width, int height) {
	assert(cmd != NULL);
	assert(tex != NULL);

	cmd->texture_subrect[0] = (float)x      / tex->width;
	cmd->texture_subrect[1] = (float)y      / tex->height;
	cmd->texture_subrect[2] = (float)width  / tex->width;
	cmd->texture_subrect[3] = (float)height / tex->height;
}

void drawcmd_set_texture_subrect_tile(drawcmd_t *cmd, texture_t *tex, int tile_width, int tile_height, int tile_x, int tile_y) {
	assert(cmd != NULL);
	assert(tex != NULL);

	const float uv_w = (float)tile_width / tex->width;
	const float uv_h = (float)tile_height / tex->height;
	cmd->texture_subrect[0] = uv_w * tile_x;
	cmd->texture_subrect[1] = uv_h * tile_y;
	cmd->texture_subrect[2] = uv_w;
	cmd->texture_subrect[3] = uv_h;
}

void drawcmd_flip_texture_subrect(drawcmd_t *cmd, int flip_x, int flip_y) {
	assert(cmd != NULL);

	if (flip_x) {
		cmd->texture_subrect[0] += cmd->texture_subrect[2];
		cmd->texture_subrect[2] = -cmd->texture_subrect[2];
	}

	if (flip_y) {
		cmd->texture_subrect[1] += cmd->texture_subrect[3];
		cmd->texture_subrect[3] = -cmd->texture_subrect[3];
	}
}

void pipeline_init(pipeline_t *pl, shader_t *shader, int commands_max) {
	assert(pl != NULL);
	assert(shader != NULL);
	assert(commands_max > 0);

	// config
	pl->components_per_vertex = 3 + 2 + 4 + 4; // pos + texcoord + color_mult + color_add
	pl->vertices_per_primitive = 6;
	int size_per_primitive = pl->components_per_vertex * sizeof(GLfloat) * pl->vertices_per_primitive;
	pl->z_sorting_enabled = 0;

	// state
	pl->commands_count = 0;
	pl->commands_max = commands_max;
	pl->texture = NULL;
	pl->shader = shader;

	glGenBuffers(1, &pl->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, pl->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, pl->commands_max * size_per_primitive, NULL, GL_DYNAMIC_DRAW);

	pl->cmd_buffer = malloc(commands_max * sizeof(*pl->cmd_buffer));

	// init attribs
	pl->attribs.a_pos = glGetAttribLocation(pl->shader->program, "a_pos");
	pl->attribs.a_texcoord = glGetAttribLocation(pl->shader->program, "a_texcoord");
	pl->attribs.a_color_mult = glGetAttribLocation(pl->shader->program, "a_color_mult");
	pl->attribs.a_color_add = glGetAttribLocation(pl->shader->program, "a_color_add");

	pipeline_set_transform(pl, GLM_MAT4_IDENTITY);
}

void pipeline_destroy(pipeline_t *pl) {
	free(pl->cmd_buffer);
	pl->cmd_buffer = NULL;

	glDeleteBuffers(1, &pl->vertex_buffer);
	if (pl->attribs.a_pos != -1) glDisableVertexAttribArray(pl->attribs.a_pos);
	if (pl->attribs.a_texcoord != -1) glDisableVertexAttribArray(pl->attribs.a_texcoord);
	if (pl->attribs.a_color_mult != -1) glDisableVertexAttribArray(pl->attribs.a_color_mult);
	if (pl->attribs.a_color_add != -1) glDisableVertexAttribArray(pl->attribs.a_color_add);
}

void pipeline_reset(pipeline_t *pl) {
	pl->commands_count = 0;
}

void pipeline_emit(pipeline_t *pl, drawcmd_t *cmd) {
	assert(pl->commands_count < pl->commands_max);

	int insert_at = pl->commands_count;
	if (pl->z_sorting_enabled) {
		// TODO: binary search or similar
		for (int i = 0; i < pl->commands_count; ++i) {
			if (cmd->position.z < pl->cmd_buffer[i].position.z) {
				insert_at = i;

				break;
			}
		}
		if (insert_at < pl->commands_count) {
			memmove(&pl->cmd_buffer[insert_at + 1], &pl->cmd_buffer[insert_at], sizeof(*pl->cmd_buffer) * (pl->commands_count - insert_at));
		}
	}

	// write into buffer
	pl->cmd_buffer[insert_at] = *cmd;
	++pl->commands_count;
}

void pipeline_set_transform(pipeline_t *pl, mat4 model) {
	assert(pl != NULL);
	assert(pl->shader != NULL);

	shader_set_uniform_mat4(pl->shader, "u_model", (float *)model);
}

void pipeline_draw(pipeline_t *pl, struct engine *engine) {
	draw_pipeline(pl, engine->u_projection, engine->u_view);
}

void pipeline_draw_ortho(pipeline_t *pl, float w, float h) {
	mat4 view;
	glm_mat4_identity(view);
	mat4 proj;
	glm_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f, proj);

	draw_pipeline(pl, proj, view);
}
