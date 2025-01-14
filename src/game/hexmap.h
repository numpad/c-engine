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

#define HEXMAP_MOVEMENT_COST_MAX 200

enum hexmap_tile_effect {
	HEXMAP_TILE_EFFECT_NONE = 0,
	HEXMAP_TILE_EFFECT_HIGHLIGHT,
	HEXMAP_TILE_EFFECT_MOVEABLE_AREA,
};

// Indicates whether a pathfind operation
// succeeded `HEXMAP_PATH_OK` or failed.
enum hexmap_path_result {
	HEXMAP_PATH_OK = 0,
	HEXMAP_PATH_ERROR,
	HEXMAP_PATH_INCOMPLETE_FLOWFIELD
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
	u8 movement_cost;
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
	usize *edges;

	// Rendering
	shader_t tile_shader;
	vec2s tile_offsets;
	model_t models[10];

	// Special tiles
	usize highlight_tile_index;
};

struct hexmap_path {
	enum hexmap_path_result result;
	struct hexcoord start;
	struct hexcoord goal;
	usize distance_in_tiles;
	usize *tiles;
};

// coordinates
int hexcoord_equal(struct hexcoord a, struct hexcoord b);
int hexmap_is_valid_coord(struct hexmap *, struct hexcoord);
int hexmap_is_valid_index(struct hexmap *, usize index);

//
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
struct hextile *hexmap_tile_at                (struct hexmap *, struct hexcoord at);

void hexmap_set_tile_effect(struct hexmap *, struct hexcoord, enum hexmap_tile_effect);
void hexmap_clear_tile_effect(struct hexmap *, enum hexmap_tile_effect);


// flowfield
void hexmap_update_edges(struct hexmap *map);
void hexmap_generate_flowfield(struct hexmap *map, struct hexcoord start_coord, usize flowfield_len, usize *flowfield);
usize hexmap_flowfield_distance(struct hexmap *map, struct hexcoord goal, usize flowfield_len, usize flowfield[flowfield_len]);
// pathfinding
enum hexmap_path_result hexmap_path_find(struct hexmap *, struct hexcoord start, struct hexcoord goal, struct hexmap_path *);
void hexmap_path_destroy(struct hexmap_path *);

int hexmap_is_tile_obstacle(struct hexmap *, struct hexcoord);

#endif

