#include "terrain.h"
#include <stdlib.h>
#include <nanovg.h>
#include <stb_ds.h>
#include "engine.h"

static inline int get_cell_state(unsigned char value, unsigned char isovalue) {
	return value >= isovalue ? 1 : 0;
}

static inline void get_cell_endpoint(int x0, int y0, int x1, int y1, int *ox, int *oy) {
	*ox = x0 + (x1 - x0) * 0.5f;
	*oy = y0 + (y1 - y0) * 0.5f;
}

void terrain_init(struct terrain_s *terrain, int w, int h) {
	terrain->width = w;
	terrain->height = h;
	terrain->density = malloc(w * h * sizeof(unsigned char *));
	terrain->isovalue = 127;
	terrain->polygon_edges = NULL;

	for (int y = 0; y < terrain->height; ++y) {
		for (int x = 0; x < terrain->width; ++x) {
			const int hash = (x * 47294) ^ y + (y * 2011) ^ x;
			terrain->density[x + y * terrain->width] = y * 5 + (hash % 255);
		}
	}
}

void terrain_destroy(struct terrain_s *terrain) {
	free(terrain->density);
}

void terrain_draw(struct terrain_s *terrain, struct engine_s *engine) {

	for (int y = 0; y < terrain->height; ++y) {
		for (int x = 0; x < terrain->width; ++x) {
			const unsigned char density = terrain->density[x + y * terrain->width];
			const float radius = ((float)density / 255.0f);
			if (density > 127) {
				nvgBeginPath(engine->vg);
				nvgCircle(engine->vg, 30.0f + x * 30.0f, 30.0f + y * 30.0f, 8.0f * radius);
				nvgFillColor(engine->vg, nvgRGB(180 - 50.0f * (1.0f - radius), 110 - 50.0f * (1.0f - radius), 100 - 50.0f * (1.0f - radius)));
				nvgFill(engine->vg);
			}
		}
	}

}

void terrain_polygonize(struct terrain_s *terrain) {
	for (int y = 0; y < terrain->height - 1; ++y) {
		for (int x = 0; x < terrain->width - 1; ++x) {
			const int cell[4] = {
				get_cell_state(terrain->density[(x + 0) + (y + 0) * terrain->width], terrain->isovalue),
				get_cell_state(terrain->density[(x + 1) + (y + 0) * terrain->width], terrain->isovalue),
				get_cell_state(terrain->density[(x + 1) + (y + 1) * terrain->width], terrain->isovalue),
				get_cell_state(terrain->density[(x + 0) + (y + 1) * terrain->width], terrain->isovalue)
			};

			int cell_type = 0;
			cell_type |= cell[0] << 0;
			cell_type |= cell[1] << 1;
			cell_type |= cell[2] << 2;
			cell_type |= cell[3] << 3;
			
		}
	}
}

