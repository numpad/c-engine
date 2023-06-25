#include "terrain.h"
#include <stdlib.h>
#include <nanovg.h>
#include "engine.h"

void terrain_init(struct terrain_s *terrain, int w, int h) {
	terrain->width = w;
	terrain->height = h;
	terrain->density = malloc(w * h * sizeof(unsigned char *));

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
			nvgBeginPath(engine->vg);
			nvgCircle(engine->vg, 30.0f + x * 30.0f, 30.0f + y * 30.0f, 8.0f * radius);
			nvgFillColor(engine->vg, nvgRGB(180 - 50.0f * (1.0f - radius), 110 - 50.0f * (1.0f - radius), 100 - 50.0f * (1.0f - radius)));
			nvgFill(engine->vg);
		}
	}

}

