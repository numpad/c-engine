#ifndef ISOTERRAIN_H
#define ISOTERRAIN_H

#include <cglm/cglm.h>
#include "gl/texture.h"
#include "gl/vbuffer.h"
#include "gl/shader.h"

typedef int iso_block;
typedef struct cJSON cJSON;


struct isoterrain_s {
	int width, height, layers;
	iso_block *blocks;

	shader_t shader;
	texture_t tileset_texture;
	vbuffer_t *vbuf;
};

// create & destroy
void isoterrain_init(struct isoterrain_s *, int w, int h, int d);
void isoterrain_init_from_file(struct isoterrain_s *, const char *path_to_script);
void isoterrain_destroy(struct isoterrain_s *);

// de-/serialization
cJSON *isoterrain_to_json(struct isoterrain_s *);

// logic
void isoterrain_draw(struct isoterrain_s *, const mat4 proj, const mat4 view);

// api
void isoterrain_get_projected_size(struct isoterrain_s *, int *width, int *height);
iso_block *isoterrain_get_block(struct isoterrain_s *, int x, int y, int z);
void isoterrain_set_block(struct isoterrain_s *, int x, int y, int z, iso_block block);
void isoterrain_pos_block_to_screen(struct isoterrain_s *, int x, int y, int z, vec2 *OUT_pos);
void isoterrain_pos_screen_to_block(struct isoterrain_s *, vec2 pos, int *OUT_x, int *OUT_y, int *OUT_z);

#endif

