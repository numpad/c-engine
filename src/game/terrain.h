#ifndef TERRAIN_H
#define TERRAIN_H

struct engine_s;

struct terrain_s {
	unsigned char *density;
	int width, height;
};

void terrain_init(struct terrain_s *terrain, int w, int h);
void terrain_destroy(struct terrain_s *terrain);

void terrain_draw(struct terrain_s *terrain, struct engine_s *engine);

#endif

