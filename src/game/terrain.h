#ifndef TERRAIN_H
#define TERRAIN_H

struct engine;

struct terrain_s {
	unsigned char isovalue;
	unsigned char *density;
	int width, height;
	
	float *polygon_edges;

	// rendering
	float x_offset, y_offset;
	float x_scale, y_scale;
};

void terrain_init(struct terrain_s *terrain, int w, int h);
void terrain_destroy(struct terrain_s *terrain);

void terrain_draw(struct terrain_s *terrain, struct engine *engine);

void terrain_polygonize(struct terrain_s *terrain);
unsigned char *terrain_density_at(struct terrain_s *terrain, int x, int y);

#endif

