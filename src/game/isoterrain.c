#include "isoterrain.h"

#include <stdlib.h>
#include <SDL_opengles2.h>
#include "gl/shader.h"
#include "gl/texture.h"
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
	terrain->texture = texture_from_image("res/image/iso_tileset.png", NULL);

	terrain->vbuf = malloc(sizeof(struct vbuffer_s));
	GLfloat vertices[] = {
		-0.25f, -0.25f,  0.0f, 0.0f,
		 0.25f, -0.25f,  1.0f, 0.0f,
		-0.25f,  0.25f,  0.0f, 15.0f/16.0f-1.0f/256.0f,
		-0.25f,  0.25f,  0.0f, 15.0f/16.0f-1.0f/256.0f,
		 0.25f, -0.25f,  1.0f, 0.0f,
		 0.25f,  0.25f,  1.0f, 15.0f/16.0f-1.0f/256.0f,
	};
	vbuffer_init(terrain->vbuf);
	vbuffer_set_data(terrain->vbuf, sizeof(vertices), vertices);
	vbuffer_set_attrib(terrain->vbuf, terrain->shader, "a_position", 2, GL_FLOAT, 4 * sizeof(float), 0);
	vbuffer_set_attrib(terrain->vbuf, terrain->shader, "a_texcoord", 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	
	glUseProgram(terrain->shader);
	glUniform1i(glGetUniformLocation(terrain->shader, "u_texture"), 0);

	// TODO: set some blocks for testing
	for (int i = 0; i < terrain->width * terrain->height; ++i) {
		terrain->blocks[i] = 0;
	}

	for (int x = 0; x < terrain->width; ++x) {
		isoterrain_set_block(terrain, x, 0, 129);
		
		if (x < 3) isoterrain_set_block(terrain, x, 1, 145);
		else isoterrain_set_block(terrain, x, 1, 129);

		if (x < 4) isoterrain_set_block(terrain, x, 2, 145);
		else isoterrain_set_block(terrain, x, 2, 129);

		for (int y = 3; y < terrain->height; ++y) {
			isoterrain_set_block(terrain, x, y, 145);
		}

		if (x > 2 && x != terrain->width - 2) {
			isoterrain_set_block(terrain, x, 4, 80);
		}
	}

	for (int y = 0; y < terrain->height; ++y) {
		if (y < 3) isoterrain_set_block(terrain, terrain->width - 2, y, 132);
		else isoterrain_set_block(terrain, terrain->width - 2, y, 148);
		if (y == 4) isoterrain_set_block(terrain, terrain->width - 2, y, 24);

		if (y < 3) isoterrain_set_block(terrain, terrain->width - 5, y, 65);
		else if (y == 3) isoterrain_set_block(terrain, terrain->width - 5, y, 81);
		else if (y == 4) isoterrain_set_block(terrain, terrain->width - 5, y, 86);

	}
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
	// draw left to right, top to bottom
	for (int x = 0; x < terrain->width; ++x) {
		for (int y = terrain->height - 1; y >= 0; --y) {
			const int i = x + y * terrain->width;

			iso_block *block = &terrain->blocks[i];
			if (*block == -1) continue;

			const float tx = *block % 16;
			const float ty = floor(*block / 15.0f + (1.0f / 256.0f));
			const float bx = (i % terrain->width) - (terrain->width / 1.0f);
			const float by = floor(i / (float)terrain->width);

			// TODO: remove
			glUniform2f(glGetUniformLocation(terrain->shader, "u_tilesize"), 1.0f / 16.0f, 1.0f / 15.0f);
		
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

