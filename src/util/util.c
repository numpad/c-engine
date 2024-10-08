#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
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

// arg parsing

int is_argv_set(int argc, char **argv, char *arg_to_check) {
	if (arg_to_check[0] == '-') ++arg_to_check;
	if (arg_to_check[0] == '-') ++arg_to_check;

	int arg_to_check_len = strnlen(arg_to_check, 64);
	for (int i = 1; i < argc; ++i) {
		char *arg = argv[i];
		int is_dashed_arg = (arg[0] == '-' && arg[1] == '-');
		if (is_dashed_arg && strncmp(arg + 2, arg_to_check, arg_to_check_len) == 0) {
			return 1;
		}
	}
	return 0;
}


// random numbers

static struct rng_state rng_state = {
	.s0 = 1234567890987654321ULL,
	.s1 = 9876543210123456789ULL,
};

static uint64_t splitmix64(uint64_t *x) {
	uint64_t z = (*x += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

static uint64_t xorshift128plus(void) {
	uint64_t x = rng_state.s0;
	uint64_t y = rng_state.s1;
	rng_state.s0 = y;
	x ^= x << 23;
	rng_state.s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
	return rng_state.s1 + y;
}

/**
 * Generate a random float in range [0..1]
 */
float rng_f(void) {
	return (float)((xorshift128plus() >> 11) * (1.0 / 9007199254740992.0));
}

/**
 * Generate a random float in range [0..1] using
 * an approximated normal/gaussian distribution.
 */
float rng_fnd(void) {
	const int iters = 12;
	float sum = 0.0f;
	for (int i = 0; i < iters; ++i) {
		sum += rng_f();
	}
	return (sum / iters);
}

void rng_seed(uint64_t seed) {
	rng_state.s0 = seed;
	rng_state.s1 = splitmix64(&rng_state.s0);
	rng_state.s0 = splitmix64(&rng_state.s0);
}

void rng_save_state(struct rng_state *state) {
	assert(state != NULL);
	*state = rng_state;
}

void rng_restore_state(struct rng_state *state) {
	rng_state = *state;
}

