#include "terrain.h"
#include <stdlib.h>
#include <SDL2_gfxPrimitives.h>
#include "engine.h"

void terrain_init(struct terrain_s *terrain, int w, int h) {
	terrain->width = w;
	terrain->height = h;
	terrain->density = malloc(w * h * sizeof(unsigned char *));

	for (int y = 0; y < terrain->height; ++y) {
		for (int x = 0; x < terrain->width; ++x) {
			terrain->density[x + y * terrain->width] = y * 5;
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
			const int radius = (density / 255.0f);
			filledCircleColor(engine->renderer, x * 10, y * 10, 10, 0xffff0000);
		}
	}
}

