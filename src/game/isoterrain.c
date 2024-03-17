#include "isoterrain.h"

#include <stdlib.h>
#include <SDL_opengles2.h>
#include <cJSON.h>
#include "util/fs.h"
#include "gl/shader.h"
#include "gl/texture.h"
#include "gl/graphics2d.h"

//
// vars
//

// tilesize in pixels
static const int texture_tile_width  = 16;
static const int texture_tile_height = 17;

//
// private functions
//

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
// public api
//

void isoterrain_init(struct isoterrain_s *terrain, int w, int h, int layers) {
	terrain->width = w;
	terrain->height = h;
	terrain->layers = layers;
	terrain->blocks = malloc(w * h * layers * sizeof(iso_block));

	isoterrain_get_projected_size(terrain, &terrain->projected_width, &terrain->projected_height);

	shader_init(&terrain->shader, "res/shader/isoterrain/vertex.glsl", "res/shader/isoterrain/fragment.glsl");
	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	texture_init_from_image(&terrain->tileset_texture, "res/environment/tiles.png", &settings);

	pipeline_init(&terrain->pipeline, &terrain->shader, 1024);
	terrain->pipeline.texture = &terrain->tileset_texture;
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
	pipeline_destroy(&terrain->pipeline);
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

void isoterrain_draw(struct isoterrain_s *terrain, engine_t *engine) {
	pipeline_reset(&terrain->pipeline);

	// draw left to right, top to bottom
	for (int iz = 0; iz < terrain->layers; ++iz) {
		for (int ix = 0; ix < terrain->width; ++ix) {
			for (int iy = terrain->height - 1; iy >= 0; --iy) {
				iso_block *block = isoterrain_get_block(terrain, ix, iy, iz);
				if (*block == -1) continue;

				// blockid to texcoord
				ivec2s tile_uv = { .x = *block % 16, .y = floor(*block / 15.0f) };
				vec3 block_pos = GLM_VEC3_ZERO_INIT;
				isoterrain_pos_block_to_screen(terrain, ix, iy, iz, block_pos);

				drawcmd_t cmd = DRAWCMD_INIT;
				cmd.size.x = texture_tile_width;
				cmd.size.y = texture_tile_height;
				cmd.position.x = block_pos[0];
				cmd.position.y = (terrain->projected_height - block_pos[1]);
				cmd.position.z = block_pos[2];
				drawcmd_set_texture_subrect_tile(&cmd, &terrain->tileset_texture, 16, 17, tile_uv.x, tile_uv.y); // tile_uv
				pipeline_emit(&terrain->pipeline, &cmd);
			}
		}
	}

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	pipeline_draw(&terrain->pipeline, engine);
}

//
// api
//

void isoterrain_get_projected_size(struct isoterrain_s *terrain, int *width, int *height) {
	if (width != NULL) {
		*width = terrain->width * 16.0f;
	}
	if (height != NULL) {
		// FIXME: this was determined empirically
		*height = terrain->height * 17.0f * (1.0/3.0f) + terrain->layers * 4.0f;
	}
}

void isoterrain_set_block(struct isoterrain_s *terrain, int x, int y, int z, iso_block block) {
	if (x < 0 || y < 0 || x >= terrain->width || y >= terrain->height || z < 0 || z >= terrain->layers) return;

	int index;
	pos_to_index(terrain, x, y, z, &index);
	terrain->blocks[index] = block;
}

void isoterrain_pos_block_to_screen(struct isoterrain_s *terrain, int x, int y, int z, vec2 OUT_pos) {
	OUT_pos[0] = (x + y) * 8.0f;
	OUT_pos[1] = ((y + 2.0f * z) - x) * 4.0f;
}

void isoterrain_pos_screen_to_block(struct isoterrain_s *terrain, vec2 pos, int *OUT_x, int *OUT_y, int *OUT_z) {
}

