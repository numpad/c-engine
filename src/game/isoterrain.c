#include "isoterrain.h"

#include <stdlib.h>
#include <SDL_opengles2.h>
#include <cJSON.h>
#include "util/fs.h"
#include "gl/shader.h"
#include "gl/texture.h"
#include "gl/vbuffer.h"

static void index_to_pos(struct isoterrain_s *terrain, size_t index, int *x, int *y, int *z) {
	*z = index / (terrain->width * terrain->height);
	index -= ((*z) * terrain->width * terrain->height);
	*y = index / terrain->width;
	*x = index % terrain->height;
}

static void pos_to_index(struct isoterrain_s *terrain, int x, int y, int z, int *index) {
	*index = x + (y * terrain->width) + (z * terrain->width * terrain->height);
}

static iso_block isoterrain_get_block(struct isoterrain_s *terrain, int x, int y, int z) {
	if (x < 0 || y < 0 || z < 0 || x >= terrain->width || y >= terrain->height || z >= terrain->layers) return -1;

	const int pos = x + y * terrain->width;
	return terrain->blocks[pos];
}

//
// create & destroy
//

void isoterrain_init(struct isoterrain_s *terrain, int w, int h, int layers) {
	terrain->width = w;
	terrain->height = h;
	terrain->layers = layers;
	terrain->blocks = malloc(w * h * layers * sizeof(iso_block));

	terrain->shader = shader_new("res/shader/isoterrain/vertex.glsl", "res/shader/isoterrain/fragment.glsl");
	terrain->texture = texture_from_image("res/image/iso_tileset.png", NULL);

	terrain->vbuf = malloc(sizeof(struct vbuffer_s));
	GLfloat vertices[] = {
		-0.25f, -0.25f,  0.0f, 0.0f,
		 0.25f, -0.25f,  1.0f, 0.0f,
		-0.25f,  0.25f,  0.0f, 16.0f/17.0f,
		-0.25f,  0.25f,  0.0f, 16.0f/17.0f,
		 0.25f, -0.25f,  1.0f, 0.0f,
		 0.25f,  0.25f,  1.0f, 16.0f/17.0f,
	};
	vbuffer_init(terrain->vbuf);
	vbuffer_set_data(terrain->vbuf, sizeof(vertices), vertices);
	vbuffer_set_attrib(terrain->vbuf, terrain->shader, "a_position", 2, GL_FLOAT, 4 * sizeof(float), 0);
	vbuffer_set_attrib(terrain->vbuf, terrain->shader, "a_texcoord", 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	
	glUseProgram(terrain->shader);
	glUniform1i(glGetUniformLocation(terrain->shader, "u_texture"), 0);
}

void isoterrain_init_from_file(struct isoterrain_s *terrain, const char *path_to_script) {
	// read map metadata
	char *file;
	long file_len;
	if (fs_readfile(path_to_script, &file, &file_len) != FS_OK) {
		fprintf(stderr, "could not read '%s'!\n", path_to_script);
		return;
	}

	cJSON *json = cJSON_Parse(file);
	free(file);

	const cJSON *width = cJSON_GetObjectItem(json, "width");
	const cJSON *height = cJSON_GetObjectItem(json, "height");
	const cJSON *layers = cJSON_GetObjectItem(json, "layers");
	const cJSON *blocks = cJSON_GetObjectItem(json, "blocks");

	if (!cJSON_IsNumber(width) || !cJSON_IsNumber(height) || !cJSON_IsNumber(layers) || !cJSON_IsArray(blocks)) {
		fprintf(stderr, "could not parse map dimensions.\n");
		return;
	}

	// actual initialization
	isoterrain_init(terrain, width->valueint, height->valueint, layers->valueint);

	// fill map
	size_t index = 0;
	const cJSON *block;
	cJSON_ArrayForEach(block, blocks) {
		if (cJSON_IsNumber(block)) {
			terrain->blocks[index] = block->valueint;
		}

		++index;
	}

	cJSON_Delete(json);
}

void isoterrain_destroy(struct isoterrain_s *terrain) {
	free(terrain->blocks);
	vbuffer_destroy(terrain->vbuf);
	free(terrain->vbuf);
}

//
// logic
//

void isoterrain_draw(struct isoterrain_s *terrain, const mat4 proj, const mat4 view) {
	glUseProgram(terrain->shader);

	glUniformMatrix4fv(glGetUniformLocation(terrain->shader, "u_projection"), 1, GL_FALSE, proj[0]);
	glUniformMatrix4fv(glGetUniformLocation(terrain->shader, "u_view"), 1, GL_FALSE, view[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, terrain->texture);
	// draw left to right, top to bottom
	for (int z = 0; z < terrain->layers; ++z) {
		for (int x = 0; x < terrain->width; ++x) {
			for (int y = terrain->height - 1; y >= 0; --y) {
				int index;
				pos_to_index(terrain, x, y, z, &index);

				iso_block *block = &terrain->blocks[index];
				if (*block == -1) continue;

				const float tx = *block % 16;
				const float ty = floor(*block / 15.0f + (1.0f / 256.0f));
				const float bx = (float)(x - z) - terrain->width * 0.9f;
				const float by = (float)y + z;
				const float bz = 0.0f;

				// TODO: remove
				glUniform2f(glGetUniformLocation(terrain->shader, "u_tilesize"), (1.0f / 256.0f) * 16.0f, (1.0f / 256.0f) * 17.0f);
			
				glUniform2f(glGetUniformLocation(terrain->shader, "u_tilepos"), tx, ty);
				glUniform3f(glGetUniformLocation(terrain->shader, "u_pos"), bx, by, bz);
				vbuffer_draw(terrain->vbuf, 6);
			}
		}
	}
}

//
// api
//

void isoterrain_set_block(struct isoterrain_s *terrain, int x, int y, int z, iso_block block) {
	if (x < 0 || y < 0 || x >= terrain->width || y >= terrain->height || z < 0 || z >= terrain->layers) return;

	int index;
	pos_to_index(terrain, x, y, z, &index);
	terrain->blocks[index] = block;
}

void isoterrain_pos_block_to_screen(struct isoterrain_s *, int x, int y, int z, vec2 *OUT_pos) {
}

void isoterrain_pos_screen_to_block(struct isoterrain_s *, vec2 pos, int *OUT_x, int *OUT_y, int *OUT_z) {
}

