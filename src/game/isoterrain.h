#ifndef ISOTERRAIN_H
#define ISOTERRAIN_H

#include <cglm/cglm.h>

typedef int iso_block;

struct vbuffer_s;

struct isoterrain_s {
	int width, height, layers;
	iso_block *blocks;

	unsigned int shader;
	unsigned int texture;
	struct vbuffer_s *vbuf;
	
};

// create & destroy
void isoterrain_init(struct isoterrain_s *, int w, int h, int d);
void isoterrain_init_from_file(struct isoterrain_s *, const char *path_to_script);
void isoterrain_destroy(struct isoterrain_s *);

// logic
void isoterrain_draw(struct isoterrain_s *, const mat4 proj, const mat4 view);

// api
iso_block *isoterrain_get_block(struct isoterrain_s *, int x, int y, int z);
void isoterrain_set_block(struct isoterrain_s *, int x, int y, int z, iso_block block);
void isoterrain_pos_block_to_screen(struct isoterrain_s *, int x, int y, int z, vec2 *OUT_pos);
void isoterrain_pos_screen_to_block(struct isoterrain_s *, vec2 pos, int *OUT_x, int *OUT_y, int *OUT_z);

#endif

