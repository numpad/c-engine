#ifndef HEXMAP_H
#define HEXMAP_H

#include <cglm/vec2.h>
#include <cglm/vec3.h>
#include "util/util.h"
#include "gl/camera.h"
#include "gl/model.h"
#include "gl/shader.h"

struct engine;

#define HEXMAP_MAX_EDGES     6
#define HEXMAP_MAX_NEIGHBORS 6

enum hexmap_tile_effect {
	HEXMAP_TILE_EFFECT_NONE = 0,
	HEXMAP_TILE_EFFECT_HIGHLIGHT,
	HEXMAP_TILE_EFFECT_MOVEABLE_AREA,
};

enum hexmap_path_result {
	HEXMAP_PATH_OK,
	HEXMAP_PATH_INVALID_COORDINATES
};

enum hexmap_neighbor {
	HEXMAP_E,
	HEXMAP_SE,
	HEXMAP_SW,
	HEXMAP_W,
	HEXMAP_NW,
	HEXMAP_NE,
	// iterators
	HEXMAP_N_FIRST = HEXMAP_E,
	HEXMAP_N_LAST  = HEXMAP_NE
};

struct hextile {
	u16 tile;
	i16 rotation;
	u8 highlight;
};

struct hexcoord {
	int x;
	int y;
};

struct hexmap {
	// General
	int w, h;
	float tilesize;

	// Tiles
	struct hextile *tiles;
	usize *tiles_flowmap;
	usize *edges;

	// Rendering
	shader_t tile_shader;
	vec2s tile_offsets;
	model_t models[10];

	// Special tiles
	usize highlight_tile_index;
};

// coordinates
int hexcoord_equal(struct hexcoord a, struct hexcoord b);
int hexmap_is_valid_coord(struct hexmap *, struct hexcoord);
int hexmap_is_valid_index(struct hexmap *, usize index);

void hexmap_init(struct hexmap *, struct engine *);
void hexmap_destroy(struct hexmap *);
void hexmap_draw(struct hexmap *, struct camera *, vec3 player_pos);

// coordinate systems
vec2s           hexmap_index_to_world_position(struct hexmap *, usize index);
struct hexcoord hexmap_index_to_coord         (struct hexmap *, usize index);
vec2s           hexmap_coord_to_world_position(struct hexmap *, struct hexcoord);
usize           hexmap_coord_to_index         (struct hexmap *, struct hexcoord);
usize           hexmap_world_position_to_index(struct hexmap *, vec2s position);
struct hexcoord hexmap_world_position_to_coord(struct hexmap *, vec2s position);
struct hexcoord hexmap_get_neighbor_coord     (struct hexmap *, struct hexcoord tile, enum hexmap_neighbor neighbor);

void hexmap_set_tile_effect(struct hexmap *, struct hexcoord, enum hexmap_tile_effect);
void hexmap_clear_tile_effect(struct hexmap *, enum hexmap_tile_effect);

// pathfinding
void hexmap_generate_flowfield(struct hexmap *map, struct hexcoord start_coord, usize flowfield_len, usize *flowfield);
enum hexmap_path_result hexmap_find_path(struct hexmap *, struct hexcoord start, struct hexcoord goal);
usize hexmap_flowfield_distance(struct hexmap *map, struct hexcoord goal, usize flowfield_len, usize flowfield[flowfield_len]);

int hexmap_is_tile_obstacle(struct hexmap *, struct hexcoord);

#endif

