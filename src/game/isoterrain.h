#ifndef ISOTERRAIN_H
#define ISOTERRAIN_H

#include <cglm/cglm.h>

typedef int iso_block;

struct vbuffer_s;

struct isoterrain_s {
	int width, height, depth;
	iso_block *blocks;

	unsigned int shader;
	unsigned int texture;
	struct vbuffer_s *vbuf;
	
};

void isoterrain_init(struct isoterrain_s *, int w, int h, int d);
void isoterrain_init_from_file(struct isoterrain_s *, const char *path_to_script);
void isoterrain_destroy(struct isoterrain_s *);

void isoterrain_draw(struct isoterrain_s *, const mat4 proj, const mat4 view);

void isoterrain_set_block(struct isoterrain_s *, int x, int y, int z, iso_block block);
#endif

