#include "game/hexmap.h"

#include <assert.h>
#include <cglm/cglm.h>
#include "gl/camera.h"
#include "gl/shader.h"


/////////////
// PRIVATE //
/////////////

static void load_hextile_models(struct hexmap *);

////////////
// PUBLIC //
////////////

int hexcoord_equal(struct hexcoord a, struct hexcoord b) {
	return a.x == b.x && a.y == b.y;
}

int hexmap_is_valid_coord(struct hexmap *map, struct hexcoord coord) {
	return (coord.x >= 0 && coord.y >= 0 && coord.x < map->w && coord.y < map->h);
}

void hexmap_init(struct hexmap *map) {
	map->w = 7;
	map->h = 9;
	map->tilesize = 115.0f;
	map->tiles = calloc(map->w * map->h, sizeof(*map->tiles));
	map->tiles_flowmap = malloc(map->w * map->h * sizeof(*map->tiles_flowmap));
	map->edges = malloc(map->w * map->h * sizeof(*map->edges) * HEXMAP_MAX_EDGES);
	map->highlight_tile_index = (usize)-1;
	shader_init_from_dir(&map->tile_shader, "res/shader/model/hexmap_tile/");

	// Make some map
#define M(x, y, T, R) map->tiles[x + map->w * y].tile = T; map->tiles[x + map->w * y].rotation = R;
	M(0, 0, 1,  0);
	M(1, 0, 4, -1);
	M(2, 0, 6, -1);
	M(0, 1, 1,  0);
	M(1, 1, 1,  0);
	M(2, 1, 4, -2);
	M(0, 2, 1,  0);
	M(1, 2, 5, -3);
	M(2, 2, 6, -2);
	M(0, 3, 3, -4);
	M(1, 3, 4, -3);
	M(2, 3, 6, -2);
	M(0, 4, 6, -3);

	M(4, 0, 8,  1);
	M(4, 1, 8, -2);
	M(4, 2, 9, -1);
	M(5, 2, 7,  0);
	M(6, 2, 7,  0);
	M(5, 3, 8,  1);
	M(4, 4, 7, -2);
	M(4, 5, 8, -2);
	M(4, 6, 8,  1);
	M(4, 7, 8, -2);
	M(4, 8, 7, -1);
#undef M

	// precomputed
	map->tile_offsets = (vec2s){
		.x = sqrtf(3.f) * map->tilesize,
		.y = (3.f / 2.f) * map->tilesize,
	};

	load_hextile_models(map);

	// Generate pathfinding data
	for (usize i = 0; i < (usize)map->w * map->h; ++i) {
		for (usize edge_i = 0; edge_i < HEXMAP_MAX_EDGES; ++edge_i) {
			map->edges[i + (i * edge_i)] = (usize)-1;
		}
	}
	usize n_tiles = (usize)map->w * map->h;
	for (int y = 0; y < map->h; ++y) {
		for (int x = 0; x < map->w; ++x) {
			uint edges_generated = 0;

			int x_nw = (y % 2 == 0) ? x : x - 1;
			int x_ne = x_nw + 1;
			int x_sw = (y % 2 == 0) ? x : x - 1;
			int x_se = x_sw + 1;

			// TODO: remove. test unwalkable tiles.
			//       not completely enough, can still move to this tile.
			struct hexcoord coord = { .x=x, .y=y };
			if (hexmap_is_tile_obstacle(map, coord)) {
				continue;
			}

			// Neighbors
			struct hexcoord tile_nw = (struct hexcoord){ .x=x_nw, .y=y-1 };
			struct hexcoord tile_ne = (struct hexcoord){ .x=x_ne, .y=y-1 };
			struct hexcoord tile_w  = (struct hexcoord){ .x=x-1,  .y=y  };
			struct hexcoord tile_e  = (struct hexcoord){ .x=x+1,  .y=y  };
			struct hexcoord tile_sw = (struct hexcoord){ .x=x_sw, .y=y+1 };
			struct hexcoord tile_se = (struct hexcoord){ .x=x_se, .y=y+1 };
			int tile_nw_reachable = hexmap_is_valid_coord(map, tile_nw) && !hexmap_is_tile_obstacle(map, tile_nw);
			int tile_ne_reachable = hexmap_is_valid_coord(map, tile_ne) && !hexmap_is_tile_obstacle(map, tile_ne);
			int tile_w_reachable  = hexmap_is_valid_coord(map, tile_w)  && !hexmap_is_tile_obstacle(map, tile_w);
			int tile_e_reachable  = hexmap_is_valid_coord(map, tile_e)  && !hexmap_is_tile_obstacle(map, tile_e);
			int tile_sw_reachable = hexmap_is_valid_coord(map, tile_sw) && !hexmap_is_tile_obstacle(map, tile_sw);
			int tile_se_reachable = hexmap_is_valid_coord(map, tile_se) && !hexmap_is_tile_obstacle(map, tile_se);

			if (tile_nw_reachable) map->edges[x + y * map->w + edges_generated++ * n_tiles] = hexmap_coord_to_index(map, tile_nw);
			if (tile_ne_reachable) map->edges[x + y * map->w + edges_generated++ * n_tiles] = hexmap_coord_to_index(map, tile_ne);
			if (tile_w_reachable)  map->edges[x + y * map->w + edges_generated++ * n_tiles] = hexmap_coord_to_index(map, tile_w);
			if (tile_e_reachable)  map->edges[x + y * map->w + edges_generated++ * n_tiles] = hexmap_coord_to_index(map, tile_e);
			if (tile_sw_reachable) map->edges[x + y * map->w + edges_generated++ * n_tiles] = hexmap_coord_to_index(map, tile_sw);
			if (tile_se_reachable) map->edges[x + y * map->w + edges_generated++ * n_tiles] = hexmap_coord_to_index(map, tile_se);
		}
	}
}

void hexmap_destroy(struct hexmap *map) {
	assert(map != NULL);
	free(map->tiles);
}

vec2s hexmap_coord_to_world_position(struct hexmap *map, struct hexcoord coord) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, coord));
	return hexmap_index_to_world_position(map, coord.x + coord.y * map->w);
}

usize hexmap_coord_to_index(struct hexmap *map, struct hexcoord coord) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, coord));
	return coord.x + coord.y * map->w;
}

vec2s hexmap_index_to_world_position(struct hexmap *map, usize index) {
	assert(map != NULL);
	assert(index < (usize)map->w * map->h);

	float horiz = map->tile_offsets.x;
	float vert = map->tile_offsets.y;

	int q = index % map->w;
	int r = floorf(index / (float)map->w);
	float horiz_offset = (r % 2 == 0) ? horiz * 0.5f : 0.0f;

	return (vec2s){
		.x = q * horiz + horiz_offset,
		.y = r * vert,
	};
}

usize hexmap_world_position_to_index(struct hexmap *map, vec2s position) {
	assert(map != NULL);

	usize y = (position.y + map->tile_offsets.y * 0.5f) / map->tile_offsets.y;
	float x_offset = (y % 2 == 0) ? 0.0f : map->tile_offsets.x * 0.5f;
	usize x = (position.x + x_offset) / map->tile_offsets.x;

	return x + y * map->w;
}

struct hexcoord hexmap_world_position_to_coord(struct hexmap *map, vec2s position) {
	assert(map != NULL);

	usize y = (position.y + map->tile_offsets.y * 0.5f) / map->tile_offsets.y;
	float x_offset = (y % 2 == 0) ? 0.0f : map->tile_offsets.x * 0.5f;
	usize x = (position.x + x_offset) / map->tile_offsets.x;

	return (struct hexcoord){ .x=x, .y=y };
}

void hexmap_draw(struct hexmap *map, struct camera *camera) {
	usize n_tiles = map->w * map->h;

	// Highlight tile.
	//usize index = map->highlight_tile_index;
	//if (index < (usize)map->w * map->h) {
	//	vec2s wp = hexmap_index_to_world_position(map, index);
	//	{
	//		mat4 model = GLM_MAT4_IDENTITY_INIT;
	//		glm_translate(model, (vec3){ wp.x, 40.0f, wp.y });
	//		glm_scale(model, (vec3){ 100.0f, 100.0f, 100.0f});
	//		model_draw(&map->models[1], &map->tile_shader, camera, model);
	//	}
	//}
	
	shader_use(&map->tile_shader);
	for (usize i = 0; i < n_tiles; ++i) {
		vec2s pos = hexmap_index_to_world_position(map, i);
		
		mat4 model = GLM_MAT4_IDENTITY_INIT;
		//glm_translate(model, (vec3){ -map->tile_offsets.x * 3.f, 0.0f, map->tile_offsets.y * -2.0f });
		glm_translate(model, (vec3){ pos.x, 0.0f, pos.y });
		glm_rotate_y(model, map->tiles[i].rotation * glm_rad(60.0f), model);
		glm_scale(model, (vec3){ 100.0f, 100.0f, 100.0f});

		// Scale highlighted tile.
		if (i == map->highlight_tile_index) {
			glm_scale_uni(model, 0.8f);
		}

		mat4 modelView = GLM_MAT4_IDENTITY_INIT;
		glm_mat4_mul(camera->view, model, modelView);
		mat3 normalMatrix = GLM_MAT3_IDENTITY_INIT;
		glm_mat4_pick3(modelView, normalMatrix);
		glm_mat3_inv(normalMatrix, normalMatrix);
		glm_mat3_transpose(normalMatrix);

		usize model_index = map->tiles[i].tile;
		shader_set_uniform_mat3(&map->tile_shader, "u_normalMatrix", (float*)normalMatrix);
		shader_set_uniform_float(&map->tile_shader, "u_highlight", map->tiles[i].highlight);
		model_draw(&map->models[model_index], &map->tile_shader, camera, model);
		// Draw water for waterless coast tiles
		if (model_index >= 2 && model_index <= 6) {
			model_draw(&map->models[1], &map->tile_shader, camera, model);
		}
	}
}

void hexmap_set_tile_effect(struct hexmap *map, usize index, enum hexmap_tile_effect effect) {
	assert(map != NULL);
	assert(index < (usize)map->w * map->h);
	switch (effect) {
	case HEXMAP_TILE_EFFECT_HIGHLIGHT:
		map->highlight_tile_index = index;
		map->tiles[index].highlight = 1;
		break;
	}
}

enum hexmap_path_result hexmap_find_path(struct hexmap *map, struct hexcoord start_coord, struct hexcoord goal_coord) {
	assert(map != NULL);

	usize map_size = (usize)map->w * map->h;
	if (!hexmap_is_valid_coord(map, start_coord) || !hexmap_is_valid_coord(map, goal_coord)) {
		for (usize i = 0; i < map_size; ++i) {
			map->tiles_flowmap[i] = (usize)-2;
		}
		return HEXMAP_PATH_INVALID_COORDINATES;
	}

	if (start_coord.x == goal_coord.x && start_coord.y == goal_coord.y) {
		return HEXMAP_PATH_OK;
	}

	const usize NONE = (usize)-2;
	const usize NOT_VISITED = (usize)-1;
	usize start = hexmap_coord_to_index(map, start_coord);
	usize goal  = hexmap_coord_to_index(map, goal_coord);

	RINGBUFFER(usize, frontier, map_size);
	RINGBUFFER_APPEND(frontier, start);

	usize came_from[map_size];
	for (usize i = 0; i < map_size; ++i)
		came_from[i] = NOT_VISITED;
	came_from[start] = NONE;

	usize NUMBER_OF_ITERATIONS = 0;
	while (frontier.len > 0) {
		usize current_node_i = RINGBUFFER_CONSUME(frontier);

		// Early Exit
		if (current_node_i == goal) {
			break;
		}

		// Check all neighboring nodes.
		usize edge_i = 0;
		while (map->edges[current_node_i + edge_i * map_size] < map_size && edge_i < HEXMAP_MAX_EDGES) {
			usize next_i = map->edges[current_node_i + edge_i * map_size];

			if (came_from[next_i] == NOT_VISITED) {
				RINGBUFFER_APPEND(frontier, next_i);
				came_from[next_i] = current_node_i;
			}
			
			++edge_i;
		}

		assert(NUMBER_OF_ITERATIONS++ < map_size);
	}

	// copy output, not good design...
	for (usize i = 0; i < map_size; ++i) {
		map->tiles_flowmap[i] = came_from[i];
	}

	return HEXMAP_PATH_OK;
}

int hexmap_is_tile_obstacle(struct hexmap *map, struct hexcoord coord) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, coord));
	usize i = hexmap_coord_to_index(map, coord);
	return (i == 0 || i == 1 || i == 7 || i == 8 || i == 9 || i == 14
			|| i == 15 || i == 21 || i == 22 || i == 24 || i == 31);
}

////////////
// STATIC //
////////////

static void load_hextile_models(struct hexmap *map) {
	const char *models[] = {
		"res/models/tiles/base/hex_grass.gltf",
		"res/models/tiles/base/hex_water.gltf",
		"res/models/tiles/coast/waterless/hex_coast_A_waterless.gltf",
		"res/models/tiles/coast/waterless/hex_coast_B_waterless.gltf",
		"res/models/tiles/coast/waterless/hex_coast_C_waterless.gltf",
		"res/models/tiles/coast/waterless/hex_coast_D_waterless.gltf",
		"res/models/tiles/coast/waterless/hex_coast_E_waterless.gltf",
		"res/models/tiles/roads/hex_road_A.gltf",
		"res/models/tiles/roads/hex_road_B.gltf",
		"res/models/tiles/roads/hex_road_E.gltf",
	};

	for (uint i = 0; i < count_of(models); ++i) {
		int err = model_init_from_file(&map->models[i], models[i]);
		assert(err == 0);
	}
}

