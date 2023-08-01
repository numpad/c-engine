#ifndef WORMS_H
#define WORMS_H

#include "scene.h"

typedef struct {
	float x, y;
	float vx, vy;
	float r;
	int fixed;
} pos_s;

struct battlebadgers_s {
	struct scene_s base;

	int bg_shader;
	unsigned int bg_texture;
	unsigned int bg_vbo;

	pos_s *balls;
	unsigned int balls_len;
};

void battlebadgers_init(struct battlebadgers_s *, struct engine_s *);

#endif

