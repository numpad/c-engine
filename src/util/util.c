#include "util.h"

#include <assert.h>

int point_in_rect(float px, float py, float x, float y, float w, float h) {
	return (px >= x && py >= y && px <= x + w && py <= y + h);
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

