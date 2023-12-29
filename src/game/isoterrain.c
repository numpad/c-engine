#include "isoterrain.h"

#include <stdlib.h>
#include <SDL_opengles2.h>
#include <cJSON.h>
#include "util/fs.h"
#include "gl/shader.h"
#include "gl/texture.h"
#include "gl/vbuffer.h"

static void view_to_block(struct isoterrain_s *terrain, int *ox, int *oy, int *oz) {
}

static void index_to_pos(struct isoterrain_s *terrain, size_t index, int *x, int *y, int *z) {
	*z = index / (terrain->width * terrain->height);
	index -= ((*z) * terrain->width * terrain->height);
	*y = index / terrain->width;
	*x = index % terrain->height;
}

static void pos_to_index(struct isoterrain_s *terrain, int x, int y, int z, int *index) {
	*index = x + (y * terrain->width) + (z * terrain->width * terrain->height);
}

iso_block *isoterrain_get_block(struct isoterrain_s *terrain, int x, int y, int z) {
	if (x < 0 || y < 0 || z < 0 || x >= terrain->width || y >= terrain->height || z >= terrain->layers) {
		return NULL;
	}

	int index;
	pos_to_index(terrain, x, y, z, &index);
	return &terrain->blocks[index];
}

//
// create & destroy
//

void isoterrain_init(struct isoterrain_s *terrain, int w, int h, int layers) {
	terrain->width = w;
	terrain->height = h;
	terrain->layers = layers;
	terrain->blocks = malloc(w * h * layers * sizeof(iso_block));

	shader_init(&terrain->shader, "res/shader/isoterrain/vertex.glsl", "res/shader/isoterrain/fragment.glsl");
	texture_init_from_image(&terrain->tileset_texture, "res/environment/tiles.png", NULL);

	terrain->vbuf = malloc(sizeof(struct vbuffer_s));
	GLfloat vertices[] = {
		0.0f * (16.0f), 0.0f * (17.0f),  0.0f, 0.0f,
		1.0f * (16.0f), 0.0f * (17.0f),  1.0f, 0.0f,
		0.0f * (16.0f), 1.0f * (17.0f),  0.0f, 1.0f,
		0.0f * (16.0f), 1.0f * (17.0f),  0.0f, 1.0f,
		1.0f * (16.0f), 0.0f * (17.0f),  1.0f, 0.0f,
		1.0f * (16.0f), 1.0f * (17.0f),  1.0f, 1.0f,
	};
	vbuffer_init(terrain->vbuf);
	vbuffer_set_data(terrain->vbuf, sizeof(vertices), vertices);
	vbuffer_set_attrib(terrain->vbuf, &terrain->shader, "a_position", 2, GL_FLOAT, 4 * sizeof(float), 0);
	vbuffer_set_attrib(terrain->vbuf, &terrain->shader, "a_texcoord", 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	
	glUseProgram(terrain->shader.program);
	shader_set_uniform_texture(&terrain->shader, "u_texture", GL_TEXTURE0, &terrain->tileset_texture);
	shader_set_uniform_vec2(&terrain->shader, "u_tilesize", (vec2){
		(1.0f / terrain->tileset_texture.width) * 16.0f,
		(1.0f / terrain->tileset_texture.height) * 17.0f,
	});
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
// de-/serialization
//

cJSON *isoterrain_to_json(struct isoterrain_s *terrain) {
	cJSON *width = cJSON_CreateNumber(terrain->width);
	cJSON *height = cJSON_CreateNumber(terrain->height);
	cJSON *layers = cJSON_CreateNumber(terrain->layers);
	cJSON *blocks = cJSON_CreateArray();
	
	cJSON *output = cJSON_CreateObject();
	cJSON_AddItemToObject(output, "width", width);
	cJSON_AddItemToObject(output, "height", height);
	cJSON_AddItemToObject(output, "layers", layers);
	cJSON_AddItemToObject(output, "blocks", blocks);

	for (ssize_t i = 0; i < terrain->width * terrain->height * terrain->layers; ++i) {
		cJSON *block_id = cJSON_CreateNumber(terrain->blocks[i]);
		cJSON_AddItemToArray(blocks, block_id);
	}

	return output;
}

//
// logic
//

void isoterrain_draw(struct isoterrain_s *terrain, const mat4 proj, const mat4 view) {
	const float projected_height = terrain->height * 17.0f * 0.334f + terrain->layers * 4.0f;

	glUseProgram(terrain->shader.program);
	shader_set_uniform_mat4(&terrain->shader, "u_projection", proj);
	shader_set_uniform_mat4(&terrain->shader, "u_view", view);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, terrain->tileset_texture.texture);
	// draw left to right, top to bottom
	for (int iz = 0; iz < terrain->layers; ++iz) {
		for (int ix = 0; ix < terrain->width; ++ix) {
			for (int iy = terrain->height - 1; iy >= 0; --iy) {
				iso_block *block = isoterrain_get_block(terrain, ix, iy, iz);
				if (*block == -1) continue;

				const float x = ix;
				const float y = iy;
				const float z = iz;

				// blockid to texcoord
				vec2 tile_uv = { *block % 16, floor(*block / 15.0f) };
				vec3 block_pos = {
					(x + y) * 8.0f,
					((y + 2.0f * z) - x) * 4.0f - projected_height,
					0.0f,
				};

				shader_set_uniform_vec2(&terrain->shader, "u_tilepos", tile_uv);
				shader_set_uniform_vec3(&terrain->shader, "u_pos", block_pos);

				vbuffer_draw(terrain->vbuf, 6);

				/*
				struct draw_cmd_s cmd = draw_cmd_new();
				cmd.vertices = terrain->vbuf;
				cmd.vertices_count = 6;
				cmd.shader = terrain->shader;
				cmd.texture[0] = terrain->texture;

				draw_cmd_queue(engine, cmd);
				*/
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

