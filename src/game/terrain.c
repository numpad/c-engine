#include "terrain.h"
#include <math.h>
#include <stdlib.h>
#include <nanovg.h>
#include <stb_ds.h>
#include <stb_perlin.h>
#include <time.h>
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
	terrain->density = malloc(w * h * sizeof(unsigned char));
	terrain->isovalue = 127;
	terrain->polygon_edges = NULL;
	// rendering
	terrain->x_offset = 0.0f;
	terrain->y_offset = 0.0f;
	terrain->x_scale = 16.0f;
	terrain->y_scale = 16.0f;

	const int seed = time(NULL);
	for (int y = 0; y < terrain->height; ++y) {
		for (int x = 0; x < terrain->width; ++x) {
			const float sx = x / (float)terrain->width;
			const float sy = y / (float)terrain->height;

			float d = (
				  0.3f * (1.0f - stb_perlin_noise3_seed(sx * 4.0f, sy * 8.0f, 0.0f, 0, 0, 0, seed))
				+ 0.7f * (1.0f - stb_perlin_noise3_seed(sx * 4.0f, sy * 16.0f, 0.2f, 0, 0, 0, seed))
			);
			if (sy > 0.85f) d = sy;

			*terrain_density_at(terrain, x, y) = 255 * fmin(d, 255.0f);
		}
	}

	terrain_polygonize(terrain);
}

void terrain_destroy(struct terrain_s *terrain) {
	free(terrain->density);
	stbds_arrfree(terrain->polygon_edges);
}

void terrain_draw(struct terrain_s *terrain, struct engine_s *engine) {
	for (int y = 0; y < terrain->height; ++y) {
		for (int x = 0; x < terrain->width; ++x) {
			const unsigned char density = *terrain_density_at(terrain, x, y);
			float radius = ((float)density / 255.0f);
			nvgBeginPath(engine->vg);
			if (get_cell_state(*terrain_density_at(terrain, x, y), terrain->isovalue) == 0) {
				radius = 0.1f;
				nvgFillColor(engine->vg, nvgRGB(200, 200, 200));
			} else {
				nvgFillColor(engine->vg, nvgRGB(180 - 50.0f * (1.0f - radius), 120 - 50.0f * (1.0f - radius), 100 - 50.0f * (1.0f - radius)));
			}
			nvgCircle(engine->vg, terrain->x_offset + x * terrain->x_scale, terrain->y_offset + y * terrain->y_scale, 8.0f * radius);
			nvgFill(engine->vg);
		}
	}

	if (terrain->polygon_edges != NULL) {
		const int len = stbds_arrlen(terrain->polygon_edges);
		for (int i = 0; i < len; i += 4) {
			const float *edges = &(terrain->polygon_edges[i]);
			nvgBeginPath(engine->vg);
			nvgMoveTo(engine->vg, terrain->x_offset + edges[0] * terrain->x_scale, terrain->y_offset + edges[1] * terrain->y_scale);
			nvgLineTo(engine->vg, terrain->x_offset + edges[2] * terrain->x_scale, terrain->y_offset + edges[3] * terrain->y_scale);
			nvgStrokeColor(engine->vg, nvgRGBAf(0.3f, 0.12f, 0.09f, 1.0f));
			nvgStrokeWidth(engine->vg, 2.0f);
			nvgStroke(engine->vg);
		}
	}

}

void terrain_polygonize(struct terrain_s *terrain) {
	for (int y = 0; y < terrain->height - 1; ++y) {
		for (int x = 0; x < terrain->width - 1; ++x) {
			const int cell[4] = {
				get_cell_state(*terrain_density_at(terrain, x + 0, y + 0), terrain->isovalue),
				get_cell_state(*terrain_density_at(terrain, x + 1, y + 0), terrain->isovalue),
				get_cell_state(*terrain_density_at(terrain, x + 1, y + 1), terrain->isovalue),
				get_cell_state(*terrain_density_at(terrain, x + 0, y + 1), terrain->isovalue)
			};
	
			float x0, y0, x1, y1;
			float x2, y2, x3, y3;
			const int cell_type = (cell[0] << 3) | (cell[1] << 2) | (cell[2] << 1) | (cell[3] << 0);
			switch (cell_type) {
			case 0:
				continue;
			case 1:
				x0 = 0.0f; y0 = 0.5f;
				x1 = 0.5f; y1 = 1.0f;
				break;
			case 2:
				x0 = 0.5f; y0 = 1.0f;
				x1 = 1.0f; y1 = 0.5f;
				break;
			case 3:
				x0 = 0.0f; y0 = 0.5f;
				x1 = 1.0f; y1 = 0.5f;
				break;
			case 4:
				x0 = 0.5f; y0 = 0.0f;
				x1 = 1.0f; y1 = 0.5f;
				break;
			case 5:
				x0 = 0.0f; y0 = 0.5f;
				x1 = 0.5f; y1 = 0.0f;
				x2 = 0.5f; y2 = 1.0f;
				x3 = 1.0f; y3 = 0.5f;
				break;
			case 6:
				x0 = 0.5f; y0 = 0.0f;
				x1 = 0.5f; y1 = 1.0f;
				break;
			case 7:
				x0 = 0.0f; y0 = 0.5f;
				x1 = 0.5f; y1 = 0.0f;
				break;
			case 8:
				x0 = 0.0f; y0 = 0.5f;
				x1 = 0.5f; y1 = 0.0f;
				break;
			case 9:
				x0 = 0.5f; y0 = 0.0f;
				x1 = 0.5f; y1 = 1.0f;
				break;
			case 10:
				x0 = 0.5f; y0 = 0.0f;
				x1 = 1.0f; y1 = 0.5f;
				x2 = 0.0f; y2 = 0.5f;
				x3 = 0.5f; y3 = 1.0f;
				break;
			case 11:
				x0 = 0.5f; y0 = 0.0f;
				x1 = 1.0f; y1 = 0.5f;
				break;
			case 12:
				x0 = 0.0f; y0 = 0.5f;
				x1 = 1.0f; y1 = 0.5f;
				break;
			case 13:
				x0 = 0.5f; y0 = 1.0f;
				x1 = 1.0f; y1 = 0.5f;
				break;
			case 14:
				x0 = 0.0f; y0 = 0.5f;
				x1 = 0.5f; y1 = 1.0f;
				break;
			default:
				continue;
			};

			switch (cell_type) {
			case 0:
			case 15:
				continue;
			case 5:
			case 10:
				stbds_arrput(terrain->polygon_edges, x + x2);
				stbds_arrput(terrain->polygon_edges, y + y2);
				stbds_arrput(terrain->polygon_edges, x + x3);
				stbds_arrput(terrain->polygon_edges, y + y3);
				// fall through
			default:
				stbds_arrput(terrain->polygon_edges, x + x0);
				stbds_arrput(terrain->polygon_edges, y + y0);
				stbds_arrput(terrain->polygon_edges, x + x1);
				stbds_arrput(terrain->polygon_edges, y + y1);
				break;
			}
		}
	}
}

unsigned char *terrain_density_at(struct terrain_s *terrain, int x, int y) {
	if (x < 0 || y < 0 || x >= terrain->width || y >= terrain->height) {
		return NULL;
	}

	return &(terrain->density[x + y * terrain->width]);
}

