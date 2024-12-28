#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <SDL.h>
#include <SDL_opengles2.h>
#include <cglm/cglm.h>
#include "input.h"
#include "engine.h"
#include "gl/camera.h"

int point_in_rect(float px, float py, float x, float y, float w, float h) {
	return (px >= x && py >= y && px <= x + w && py <= y + h);
}

int drag_in_rect(struct input_drag_s *drag, float x, float y, float w, float h) {
	return (drag->state == INPUT_DRAG_BEGIN || drag->state == INPUT_DRAG_IN_PROGRESS || drag->state == INPUT_DRAG_END)
	        && (drag->x >= x && drag->y >= y && drag->x <= x + w && drag->y <= y + h);
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

vec2s world_to_screen(float vw, float vh, mat4 projection, mat4 view, mat4 model, vec3s point) {
	mat4 mvp = GLM_MAT4_IDENTITY_INIT;
	glm_mat4_mulN((mat4 *[]){projection, view, model}, 3, mvp);

	vec4 point_world = { point.x, point.y, point.z, 1.0f };
	vec4 point_clipspace = GLM_VEC3_ZERO_INIT;
	glm_mat4_mulv(mvp, point_world, point_clipspace);

	vec3s point_ndc = (vec3s) {
		.x = point_clipspace[0] / point_clipspace[3],
		.y = point_clipspace[1] / point_clipspace[3],
		.z = point_clipspace[2] / point_clipspace[3],
	};

	return (vec2s) {
		.x = ((point_ndc.x + 1.f) * 0.5f) * vw,
		.y = ((1.f - point_ndc.y) * 0.5f) * vh
	};
}

vec2s world_to_screen_camera(struct engine *engine, struct camera *camera, mat4 model, vec3s point) {
	return world_to_screen(engine->window_width, engine->window_height, camera->projection, camera->view, model, point);
}

vec3s screen_to_world(float vw, float vh, mat4 projection, mat4 view, float screen_x, float screen_y) {
	const float floor_y = 0.0f;
	vec2s ndc = {
		.x = (2.0f * screen_x) / vw - 1.0f,
		.y = 1.0f - (2.0f * screen_y) / vh
	};

	vec4s clip_start = {{ndc.x, ndc.y, -1.0f, 1.0f}};
	vec4s clip_end = {{ndc.x, ndc.y, 1.0f, 1.0f}};

	// Convert clip space -> world space
	mat4 inv_proj_view;
	glm_mat4_mul(projection, view, inv_proj_view);
	glm_mat4_inv(inv_proj_view, inv_proj_view);

	vec4s world_start, world_end;
	glm_mat4_mulv(inv_proj_view, clip_start.raw, world_start.raw);
	glm_mat4_mulv(inv_proj_view, clip_end.raw, world_end.raw);

	// Perspective divide (w should be 1 already, but its good practice it seems)
	if (world_start.w != 0.0f) {
		glm_vec3_divs(world_start.raw, world_start.w, world_start.raw);
	}
	if (world_end.w != 0.0f) {
		glm_vec3_divs(world_end.raw, world_end.w, world_end.raw);
	}

	vec3s ray_dir;
	glm_vec3_sub(world_end.raw, world_start.raw, ray_dir.raw);
	glm_vec3_normalize(ray_dir.raw);

	float t = (floor_y - world_start.y) / ray_dir.y;
	return (vec3s) {
		.x = world_start.x + ray_dir.x * t,
		.y = floor_y,
		.z = world_start.z + ray_dir.z * t
	};
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

	assert(has_error == 0);
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


// measure performance

Uint64 profile_begin(void) {
	return SDL_GetPerformanceCounter();
}

double profile_end_ms(Uint64 begin) {
	Uint64 end = SDL_GetPerformanceCounter();
	Uint64 freq = SDL_GetPerformanceFrequency();
	double seconds_elapsed = (double)(end - begin) / freq;
	return seconds_elapsed * 1000.0;
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

int rng_i(void) {
	return (int)(xorshift128plus() & 0x7FFFFFFF);
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

