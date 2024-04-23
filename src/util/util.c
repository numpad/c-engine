#include "util.h"

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <SDL_opengles2.h>

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

int gl_check_error(const char *file, int line) {
	// TODO: only in DEBUG
    GLenum errorCode;
    int has_error = 0;
	while ((errorCode = glGetError()) != GL_NO_ERROR) {
		has_error = 1;

        const char *error = "UNKNOWN ERROR";
        switch (errorCode) {
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }

		printf("OpenGL Error %s:%d : %s\n", file, line, error);
    }

	return has_error;
}
