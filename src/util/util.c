#include "util.h"

#include <math.h>
#include <assert.h>

int point_in_rect(float px, float py, float x, float y, float w, float h) {
	return (px >= x && py >= y && px <= x + w && py <= y + h);
}

int nearest_pow2(int value) {
	assert(value >= 1);

	int power = 1;
	while (power < value) {
		power *= 2;
	}
	return power;
}

const char *str_match_bracket(const char *str, size_t len, char open, char close) {
	assert(str[0] == open);

	int depth = 0;
	for (size_t i = 0; i < len; ++i) {
		const char c = str[i];
		if (c == open) {
			++depth;
		} else if (c == close) {
			--depth;
			if (depth == 0) {
				return &str[i];
			}
		}
	}
	assert(depth == 0);

	return NULL;
}

float calculate_angle_segment(float angle, int segments) {
	assert(segments > 0);

	angle = fmodf(angle, 2.0f * M_PI);
	while (angle < 0.0f) {
		angle += 2.0f * M_PI;
	}

	float segment = (2.0f * M_PI) / segments;
	int div = (int)(angle / segment);

	return div;
}

