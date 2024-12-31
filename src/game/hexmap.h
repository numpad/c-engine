#ifndef HEXMAP_H
#define HEXMAP_H

#include <cglm/vec2.h>
#include "util/util.h"
#include "gl/camera.h"
#include "gl/model.h"

#define HEXMAP_MAX_EDGES 6

enum hexmap_tile_effect {
	HEXMAP_TILE_EFFECT_HIGHLIGHT
};

enum hexmap_path_result {
	HEXMAP_PATH_OK,
	HEXMAP_PATH_INVALID_COORDINATES
};

struct hextile {
	short tile;
	short rotation;
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
	vec2s tile_offsets;
	model_t models[10];

	// Special tiles
	usize highlight_tile_index;
};

struct hexcoord {
	int x;
	int y;
};

int hexmap_is_valid_coord(struct hexmap *, struct hexcoord);

void  hexmap_init(struct hexmap *);
void  hexmap_destroy(struct hexmap *);
void  hexmap_draw(struct hexmap *, struct camera *);

vec2s hexmap_index_to_world_position(struct hexmap *, usize index);
vec2s hexmap_coord_to_world_position(struct hexmap *, int x, int y);
usize hexmap_coord_to_index(struct hexmap *, struct hexcoord);
usize hexmap_world_position_to_index(struct hexmap *, vec2s position);
struct hexcoord hexmap_world_position_to_coord(struct hexmap *, vec2s position);

void  hexmap_set_tile_effect(struct hexmap *, usize index, enum hexmap_tile_effect);
enum hexmap_path_result hexmap_find_path(struct hexmap *, struct hexcoord start, struct hexcoord goal);

#endif

