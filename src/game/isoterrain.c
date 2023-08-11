#include "isoterrain.h"

#include <stdlib.h>
#include <SDL_opengles2.h>
#include "gl/vbuffer.h"

static iso_block isoterrain_get_block(struct isoterrain_s *terrain, int x, int y) {
	if (x < 0 || y < 0 || x >= terrain->width || y >= terrain->height) return -1;

	const int pos = x + y * terrain->width;
	return terrain->blocks[pos];
}

void isoterrain_init(struct isoterrain_s *terrain, int w, int h) {
	terrain->width = w;
	terrain->height = h;
	terrain->blocks = calloc(w * h, sizeof(iso_block));

	terrain->shader = shader_new("res/shader/isoterrain/vertex.glsl", "res/shader/isoterrain/fragment.glsl");
	terrain->texture = texture_from_image("res/image/iso-tileset.png", NULL);

	terrain->vbuf = malloc(sizeof(struct vbuffer_s));
	GLfloat vertices[] = {
		-0.25f, -0.25f,  0.0f, 0.0f,
		 0.25f, -0.25f,  1.0f, 0.0f,
		-0.25f,  0.25f,  0.0f, 1.0f,
		-0.25f,  0.25f,  0.0f, 1.0f,
		 0.25f, -0.25f,  1.0f, 0.0f,
		 0.25f,  0.25f,  1.0f, 1.0f,
	};
	vbuffer_init(terrain->vbuf);
	vbuffer_set_data(terrain->vbuf, sizeof(vertices), vertices);
	vbuffer_set_attrib(terrain->vbuf, terrain->shader, "a_position", 2, GL_FLOAT, 4 * sizeof(float), 0);
	vbuffer_set_attrib(terrain->vbuf, terrain->shader, "a_texcoord", 2, GL_FLOAT, 4 * sizeof(float), 2 * sizeof(float));
	
	glUseProgram(terrain->shader);
	glUniform2f(glGetUniformLocation(terrain->shader, "u_pos"), 0.0f, 0.0f);
	glUniform2f(glGetUniformLocation(terrain->shader, "u_tilepos"), 1.0f, 9.0f);
	glUniform2f(glGetUniformLocation(terrain->shader, "u_tilesize"), 1.0f / 11, 1.0f / 10);

	// TODO: set some blocks for testing
	for (int i = 0; i < terrain->width * terrain->height; ++i) {
		terrain->blocks[i] = 0;
	}
	isoterrain_set_block(terrain, 0, 0, 89);
	isoterrain_set_block(terrain, 1, 0, 90);
}

void isoterrain_destroy(struct isoterrain_s *terrain) {
	free(terrain->blocks);
	vbuffer_destroy(terrain->vbuf);
	free(terrain->vbuf);
}

void isoterrain_draw(struct isoterrain_s *terrain, const mat4 proj, const mat4 view) {
	glUseProgram(terrain->shader);

	glUniformMatrix4fv(glGetUniformLocation(terrain->shader, "u_projection"), 1, GL_FALSE, proj[0]);
	glUniformMatrix4fv(glGetUniformLocation(terrain->shader, "u_view"), 1, GL_FALSE, view[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, terrain->texture);
	//for (int i = (terrain->width * terrain->height) - 1; i >= 0; i--) {
	for (int x = 0; x < terrain->width; ++x) {
	//	for (int y = 0; y < terrain->height; ++y) {
	//for (int x = terrain->width - 1; x >= 0; --x) {
		for (int y = terrain->height - 1; y >= 0; --y) {
			const int i = x + y * terrain->width;

			iso_block *block = &terrain->blocks[i];
			if (*block == -1) continue;

			const float tx = *block % 11;
			const float ty = floor(*block / (float)11);
			const float bx = i % terrain->width;
			const float by = floor(i / (float)terrain->width);

			// TODO: remove
			glUniform2f(glGetUniformLocation(terrain->shader, "u_tilesize"), 1.0f / 11, 1.0f / 10);
		
			glUniform2f(glGetUniformLocation(terrain->shader, "u_tilepos"), tx, ty);
			glUniform2f(glGetUniformLocation(terrain->shader, "u_pos"), bx, by);
			vbuffer_draw(terrain->vbuf, 6);
		}
	}
}

void isoterrain_set_block(struct isoterrain_s *terrain, int x, int y, iso_block block) {
	if (x < 0 || y < 0 || x >= terrain->width || y >= terrain->height) return;

	const int pos = x + y * terrain->width;
	terrain->blocks[pos] = block;
}

