#include "game/hexmap.h"

#include <assert.h>
#include <cglm/cglm.h>
#include "gl/camera.h"
#include "gl/shader.h"
#include "engine.h"
#include "util/util.h"

/////////////
// PRIVATE //
/////////////

static const usize NO_EDGE = (usize)-1;
static const usize NODE_NONE = (usize)-2;
static const usize NODE_NOT_VISITED = (usize)-1;

static void load_hextile_models(struct hexmap *);

////////////
// PUBLIC //
////////////

int hexcoord_equal(struct hexcoord a, struct hexcoord b) {
	return a.x == b.x && a.y == b.y;
}

int hexmap_is_valid_coord(struct hexmap *map, struct hexcoord coord) {
	assert(map != NULL);
	return (coord.x >= 0 && coord.y >= 0 && coord.x < map->w && coord.y < map->h);
}

int hexmap_is_valid_index(struct hexmap *map, usize index) {
	assert(map != NULL);
	return (index < (usize)map->w * map->h);
}

void hexmap_init(struct hexmap *map, struct engine *engine) {
	map->w = 7;
	map->h = 9;
	map->tilesize = 2.0f;
	map->edges = malloc(map->w * map->h * sizeof(*map->edges) * HEXMAP_MAX_EDGES);
	map->tiles = calloc(map->w * map->h, sizeof(*map->tiles));
	for (usize i = 0; i < (usize)map->w * map->h; ++i) {
		map->tiles[i].movement_cost = 1;
		map->tiles[i].occupied_by = 0;
	}
	shader_init_from_dir(&map->tile_shader, "res/shader/model/hexmap_tile/");
	shader_set_uniform_buffer(&map->tile_shader, "Global", &engine->shader_global_ubo);

	// Make some map
#define M(x, y, T, R, M) \
	map->tiles[x + map->w * y].tile = T;     \
	map->tiles[x + map->w * y].rotation = R; \
	map->tiles[x + map->w * y].movement_cost = M;
	
	M(0, 0, 1,  0, HEXMAP_MOVEMENT_COST_MAX);
	M(1, 0, 4, -1, HEXMAP_MOVEMENT_COST_MAX);
	M(2, 0, 6, -1, 1);
	M(0, 1, 1,  0, HEXMAP_MOVEMENT_COST_MAX);
	M(1, 1, 1,  0, HEXMAP_MOVEMENT_COST_MAX);
	M(2, 1, 4, -2, HEXMAP_MOVEMENT_COST_MAX);
	M(0, 2, 1,  0, HEXMAP_MOVEMENT_COST_MAX);
	M(1, 2, 5, -3, HEXMAP_MOVEMENT_COST_MAX);
	M(2, 2, 6, -2, 1);
	M(0, 3, 3, -4, HEXMAP_MOVEMENT_COST_MAX);
	M(1, 3, 4, -3, HEXMAP_MOVEMENT_COST_MAX);
	M(2, 3, 6, -2, 1);
	M(0, 4, 6, -3, 1);

	M(4, 0, 8,  1, 1);
	M(4, 1, 8, -2, 1);
	M(4, 2, 9, -1, 1);
	M(5, 2, 7,  0, 1);
	M(6, 2, 7,  0, 1);
	M(5, 3, 8,  1, 1);
	M(4, 4, 7, -2, 1);
	M(4, 5, 8, -2, 1);
	M(4, 6, 8,  1, 1);
	M(4, 7, 8, -2, 1);
	M(4, 8, 7, -1, 1);
#undef M

	// precomputed
	map->tile_offsets = (vec2s){
		.x = sqrtf(3.f) * map->tilesize,
		.y = (3.f / 2.f) * map->tilesize,
	};

	load_hextile_models(map);

	// Generate pathfinding data
	hexmap_generate_edges(map);
}

void hexmap_destroy(struct hexmap *map) {
	assert(map != NULL);
	// TODO: Store this...
	for (usize i = 0; i < count_of(map->models); ++i) {
		model_destroy(&map->models[i]);
	}
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

struct hexcoord hexmap_index_to_coord(struct hexmap *map, usize index) {
	assert(map != NULL);
	assert(hexmap_is_valid_index(map, index));
	return (struct hexcoord){ .x=index % map->w, .y=(int)(index / (float)map->w) };
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

struct hexcoord hexmap_get_neighbor_coord(struct hexmap *map, struct hexcoord tile, enum hexmap_neighbor neighbor) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, tile));

	static const int dx_odd[] = {1, 0, -1, -1, -1, 0};
	static const int dx_even[]  = {1, 1, 0, -1, 0, 1};
	static const int dy[]  = {0, 1, 1, 0, -1, -1};

	const int *dx = (tile.y % 2 == 0) ? dx_even : dx_odd;

	struct hexcoord neighbor_coord;
	neighbor_coord.x = tile.x + dx[neighbor];
	neighbor_coord.y = tile.y + dy[neighbor];

	return neighbor_coord;
}

struct hextile *hexmap_tile_at(struct hexmap *map, struct hexcoord at) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, at));
	usize i = hexmap_coord_to_index(map, at);
	return &map->tiles[i];
}

void hexmap_draw(struct hexmap *map, struct camera *camera, vec3 player_pos) {
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
		
		// TODO: model matrices wont change often: lets cache them...
		mat4 model = GLM_MAT4_IDENTITY_INIT;
		//glm_translate(model, (vec3){ -map->tile_offsets.x * 3.f, 0.0f, map->tile_offsets.y * -2.0f });
		glm_translate(model, (vec3){ pos.x, 0.0f, pos.y });
		glm_rotate_y(model, map->tiles[i].rotation * glm_rad(60.0f), model);
		glm_scale_uni(model, 1.733f);

		mat4 modelView = GLM_MAT4_IDENTITY_INIT;
		glm_mat4_mul(camera->view, model, modelView);
		mat3 normalMatrix = GLM_MAT3_IDENTITY_INIT;
		glm_mat4_pick3(modelView, normalMatrix);
		glm_mat3_inv(normalMatrix, normalMatrix);
		glm_mat3_transpose(normalMatrix);
		// Set uniforms
		shader_set_uniform_mat3(&map->tile_shader, "u_normalMatrix", (float*)normalMatrix);
		shader_set_uniform_float(&map->tile_shader, "u_highlight", map->tiles[i].highlight);
		shader_set_uniform_vec3(&map->tile_shader, "u_player_world_pos", player_pos);

		usize model_index = map->tiles[i].tile;
		model_draw(&map->models[model_index], &map->tile_shader, camera, model);
		// Draw water for waterless coast tiles
		if (model_index >= 2 && model_index <= 6) {
			model_draw(&map->models[1], &map->tile_shader, camera, model);
		}
	}
}

void hexmap_set_tile_effect(struct hexmap *map, struct hexcoord coord, enum hexmap_tile_effect effect) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, coord) && "Invalid coord");

	usize index = hexmap_coord_to_index(map, coord);
	switch (effect) {
	case HEXMAP_TILE_EFFECT_NONE:
		map->tiles[index].highlight = 0;
		break;
	case HEXMAP_TILE_EFFECT_ATTACKABLE:
		map->tiles[index].highlight = 1;
		break;
	case HEXMAP_TILE_EFFECT_MOVEABLE_AREA:
		map->tiles[index].highlight = 2;
		break;
	}
}

void hexmap_clear_tile_effect(struct hexmap *map, enum hexmap_tile_effect effect_to_reset) {
	assert(map != NULL);
	for (usize i = 0; i < (usize)map->w * map->h; ++i) {
		if (map->tiles[i].highlight == effect_to_reset) {
			map->tiles[i].highlight = HEXMAP_TILE_EFFECT_NONE;
		}
	}
}

void hexmap_generate_edges(struct hexmap *map) {
	for (usize i = 0; i < (usize)map->w * map->h * HEXMAP_MAX_EDGES; ++i) {
		map->edges[i] = NO_EDGE;
	}
	usize n_tiles = (usize)map->w * map->h;
	for (int y = 0; y < map->h; ++y) {
		for (int x = 0; x < map->w; ++x) {
			// Neighbors
			struct hexcoord current_coord = { .x=x, .y=y };
			usize current = hexmap_coord_to_index(map, current_coord);
			// TODO: Allow leaving from HEXMAP_MOVEMENT_COST_MAX?
			//if (map->tiles[current].movement_cost >= HEXMAP_MOVEMENT_COST_MAX) {
			//	continue;
			//}
			uint edges_generated = 0;
			for (enum hexmap_neighbor i = HEXMAP_N_FIRST; i <= HEXMAP_N_LAST; ++i) {
				struct hexcoord neighbor_coord = hexmap_get_neighbor_coord(map, current_coord, i);
				if (hexmap_is_valid_coord(map, neighbor_coord)) {
					map->edges[current + edges_generated++ * n_tiles] = hexmap_coord_to_index(map, neighbor_coord);
				}
			}
		}
	}
}

void hexmap_generate_flowfield(struct hexmap *map, struct hexcoord start_coord, usize flowfield_len, usize *flowfield) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, start_coord));
	assert(flowfield != NULL);
	// TODO: this param just makes sure i allocate enough memory, meh...
	assert(flowfield_len == (usize)map->w * map->h);

	usize start = hexmap_coord_to_index(map, start_coord);

	usize map_size = (usize)map->w * map->h;
	RINGBUFFER(usize, frontier, map_size);
	RINGBUFFER_APPEND(frontier, start);

	// Readability, came_from[tile] means the index of the tile 1 step closer to the `start_coord`.
	usize *came_from = flowfield;

	for (usize i = 0; i < map_size; ++i)
		came_from[i] = NODE_NOT_VISITED;
	came_from[start] = NODE_NONE;

	usize NUMBER_OF_ITERATIONS = 0;
	while (frontier.len > 0) {
		usize current_node_i = RINGBUFFER_CONSUME(frontier);
		// Check all neighboring nodes.
		usize edge_i = 0;
		while (map->edges[current_node_i + edge_i * map_size] < map_size && edge_i < HEXMAP_MAX_EDGES) {
			usize next_i = map->edges[current_node_i + edge_i * map_size];
			++edge_i;
			if (hexmap_is_tile_obstacle(map, hexmap_index_to_coord(map, next_i))) {
				continue;
			}
			if (came_from[next_i] == NODE_NOT_VISITED) {
				RINGBUFFER_APPEND(frontier, next_i);
				came_from[next_i] = current_node_i;
			}
		}
		assert(NUMBER_OF_ITERATIONS++ < map_size);
	}
}

enum hexmap_path_result hexmap_path_find(struct hexmap *map, struct hexcoord start_coord, struct hexcoord goal_coord, struct hexmap_path *output_path) {
	return hexmap_path_find_ex(map, start_coord, goal_coord, PATH_FLAGS_NONE, output_path);
}

enum hexmap_path_result hexmap_path_find_ex(struct hexmap *map, struct hexcoord start_coord, struct hexcoord goal_coord, enum path_find_flags flags, struct hexmap_path *output_path) {
	assert(map != NULL);
	assert(output_path != NULL); // Maybe allow NULL, just to check if any path exists?
	assert((flags == PATH_FLAGS_NONE || flags == PATH_FLAGS_FIND_NEIGHBOR) && "flag not implemented?");

	output_path->distance_in_tiles = 0;
	output_path->start             = start_coord;
	output_path->goal              = goal_coord;
	output_path->result            = HEXMAP_PATH_ERROR;
	output_path->tiles             = NULL;

	// start & goal need to be valid
	const int is_valid_start = hexmap_is_valid_coord(map, start_coord);
	const int is_valid_goal  = hexmap_is_valid_coord(map, goal_coord);
	if (!is_valid_start || !is_valid_goal) {
		output_path->result = HEXMAP_PATH_ERROR;
		return output_path->result;
	}

	if (hexcoord_equal(start_coord, goal_coord)) {
		// no pathfinding needed
		output_path->result = HEXMAP_PATH_OK;
		return output_path->result;
	}

	const usize start    = hexmap_coord_to_index(map, start_coord);
	usize goal           = hexmap_coord_to_index(map, goal_coord);
	const usize map_size = (usize)map->w * map->h;
	RINGBUFFER(usize, frontier, map_size);
	RINGBUFFER_APPEND(frontier, start);

	usize came_from[map_size];
	for (usize i = 0; i < map_size; ++i)
		came_from[i] = NODE_NOT_VISITED;
	came_from[start] = NODE_NONE;

	{ // store relevant nodes in the `came_from` flowfield
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
				++edge_i;
				// If goal is a neighbor of the current tile, we update
				// the output path goal to it. No need to search further.
				if ((flags & PATH_FLAGS_FIND_NEIGHBOR) && next_i == goal) {
					goal = current_node_i;
					output_path->goal = hexmap_index_to_coord(map, current_node_i);
					goto goal_reached;
				}
				if (hexmap_is_tile_obstacle(map, hexmap_index_to_coord(map, next_i))) {
					continue;
				}
				if (came_from[next_i] == NODE_NOT_VISITED) {
					RINGBUFFER_APPEND(frontier, next_i);
					came_from[next_i] = current_node_i;
				}
			}
			assert(NUMBER_OF_ITERATIONS++ < map_size);
		}
	}
goal_reached:

	// allocate enough memory to store the longest possible path (visits each node once)
	output_path->tiles = malloc(map_size * sizeof(*output_path->tiles));

	{ // walk back to build final path
		usize walk_back_iter = goal;
		usize NUMBER_OF_ITERATIONS = 0;
		output_path->distance_in_tiles = 0;
		while (walk_back_iter != start && walk_back_iter != NODE_NOT_VISITED) {
			assert(walk_back_iter != NODE_NONE);
			assert(walk_back_iter != NODE_NOT_VISITED);
			assert(walk_back_iter < map_size);
			output_path->tiles[output_path->distance_in_tiles] = walk_back_iter;
			walk_back_iter = came_from[walk_back_iter];
			++output_path->distance_in_tiles;
			assert(NUMBER_OF_ITERATIONS++ < map_size);
		}
		if (walk_back_iter == start) {
			// We do not want to count the start tile as part of the resulting path,
			// but it might be nice to have?
			// TODO: Store the start in hexmap_find_path() result?
			output_path->tiles[output_path->distance_in_tiles] = start;
			//++output_path->distance_in_tiles;
			assert(output_path->tiles[0] == goal);
			assert(output_path->tiles[output_path->distance_in_tiles] == start);
			output_path->result = HEXMAP_PATH_OK;
		} else {
			free(output_path->tiles);
			output_path->tiles = NULL;
			output_path->distance_in_tiles = 0;
			output_path->result = (walk_back_iter == NODE_NOT_VISITED)
				? HEXMAP_PATH_INCOMPLETE_FLOWFIELD
				: HEXMAP_PATH_ERROR;
		}
	}
	return output_path->result;
}

void hexmap_path_destroy(struct hexmap_path *path) {
	assert(path != NULL);
	switch (path->result) {
	case HEXMAP_PATH_OK:
		// `path->tiles` can be NULL.
		free(path->tiles);
		break;
	case HEXMAP_PATH_ERROR:
	case HEXMAP_PATH_INCOMPLETE_FLOWFIELD:
		break;
	};
	path->tiles = NULL;
	path->distance_in_tiles = 0;
	path->result = HEXMAP_PATH_ERROR;
	path->start = path->goal = (struct hexcoord){ .x=-1, .y=-1 };
}

usize hexmap_path_at(struct hexmap_path *path, usize index) {
	assert(path != NULL);
	assert(index < path->distance_in_tiles);

	return path->tiles[path->distance_in_tiles - index - 1];
}

// Calculate the distance from the `flowfield` origin to the `goal`.
// Returns the distance in tiles in case of success.
// On error, returns (usize)-1.
usize hexmap_flowfield_distance(struct hexmap *map, struct hexcoord goal_coord, usize flowfield_len, usize flowfield[flowfield_len]) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, goal_coord));
	assert(flowfield_len == (usize)map->w * map->h);
#ifdef DEBUG
	usize none_nodes = 0;
	for (usize i = 0; i < flowfield_len; ++i) {
		//assert(flowfield[i] != NODE_NOT_VISITED);
		if (flowfield[i] == none_nodes) {
			++none_nodes;
		}
	}
	//assert(none_nodes == 1 && "Flowfield has either zero or multiple goals, allowed is exactly one.");
#endif

	const usize max_distance = (usize)map->w * map->h;

	usize goal = hexmap_coord_to_index(map, goal_coord);
	usize *came_from = flowfield;
	if (came_from[goal] == NODE_NOT_VISITED) {
		// invalid target
		return (usize)-1;
	}
	if (came_from[goal] == NODE_NONE) {
		// goal is the same as origin
		return 0;
	}

	usize index = came_from[goal];
	usize distance = 0;
	while (index != NODE_NONE && distance < max_distance) {
		index = came_from[index];
		++distance;
	}
	return distance;
}

int hexmap_is_tile_obstacle(struct hexmap *map, struct hexcoord coord) {
	assert(map != NULL);
	assert(hexmap_is_valid_coord(map, coord));
	usize i = hexmap_coord_to_index(map, coord);
	return map->tiles[i].movement_cost >= HEXMAP_MOVEMENT_COST_MAX
		|| map->tiles[i].occupied_by != 0;
}

////////////
// STATIC //
////////////

static void load_hextile_models(struct hexmap *map) {
	assert(map != NULL);
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

