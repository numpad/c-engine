#ifndef UTIL_H
#define UTIL_H

#include <SDL.h>
#include <stdint.h>
#include <stddef.h>
#include <cglm/types-struct.h>


typedef unsigned int uint;
typedef size_t usize;

// defines
#define UTIL_FOR(_i, max) for (size_t _i = 0; _i < max; ++_i)
#define count_of(arr) (sizeof(arr) / sizeof(arr[0]))
#define GL_CHECK_ERROR() gl_check_error(__FILE__, __LINE__)

#define UTIL_IS_GL_FILTER_MIPMAP(filter)      \
	(   (filter) == GL_NEAREST_MIPMAP_NEAREST \
	 || (filter) == GL_NEAREST_MIPMAP_LINEAR  \
	 || (filter) == GL_LINEAR_MIPMAP_NEAREST  \
	 || (filter) == GL_LINEAR_MIPMAP_LINEAR)

// math
int point_in_rect(float px, float py, float x, float y, float w, float h);
int nearest_pow2(int value);
// angle in radians, segments are clockwise.
float calculate_angle_segment(float angle, int segments);

// coordinate systems
vec2s world_to_screen(float vw, float vh, mat4 projection, mat4 view, mat4 model, vec3s point);

// string utils
const char *str_match_bracket(const char *str, size_t len, char open, char close);

// gl utils
int gl_check_error(const char *file, int line);

// arg parsing
int is_argv_set(int argc, char **argv, char *arg_to_check);

// measure performance
Uint64 profile_begin(void);
double profile_end_ms(Uint64 begin);
#if DEBUG
#define PROFILE \
	for (Uint64 __once = 1, begin = profile_begin(); __once--; printf("\x1b[33m[PROFILER]\x1b[0m %.1f ms at %s:%d\n", profile_end_ms(begin), __FILE__, __LINE__))
#else
#define PROFILE
#endif

// random numbers

struct rng_state {
	uint64_t s0;
	uint64_t s1;
};

float rng_f(void);
float rng_fnd(void);
void  rng_seed(uint64_t seed);

void  rng_save_state(struct rng_state *);
void  rng_restore_state(struct rng_state *);

#endif

