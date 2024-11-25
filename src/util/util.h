#ifndef UTIL_H
#define UTIL_H

#include <SDL.h>
#include <stdint.h>
#include <stddef.h>
#include <cglm/types-struct.h>

// Int types, TODO: check if "fast" is just a meme.
typedef uint_fast8_t   u8;
typedef  int_fast8_t   i8;
typedef uint_fast16_t u16;
typedef  int_fast16_t i16;
typedef uint_fast32_t u32;
typedef  int_fast32_t i32;
typedef uint_fast64_t u64;
typedef  int_fast64_t i64;

typedef unsigned char uchar;
typedef unsigned int uint;
typedef size_t usize;
typedef i64 isize;

// defines
#define UTIL_FOR(_i, max) for (size_t _i = 0; _i < max; ++_i)
#define count_of(arr) (sizeof(arr) / sizeof(arr[0]))
#define GL_CHECK_ERROR() gl_check_error(__FILE__, __LINE__)

#define UTIL_IS_GL_FILTER_MIPMAP(filter)      \
	(   (filter) == GL_NEAREST_MIPMAP_NEAREST \
	 || (filter) == GL_NEAREST_MIPMAP_LINEAR  \
	 || (filter) == GL_LINEAR_MIPMAP_NEAREST  \
	 || (filter) == GL_LINEAR_MIPMAP_LINEAR)

// Data Structures

#define RINGBUFFER(TYPE, NAME, CAPACITY) \
	TYPE __buffer_##NAME[CAPACITY];                              \
	struct { usize head, len, capacity; TYPE *items; } NAME = {  \
		.head=0, .len=0, .capacity=CAPACITY, .items = (TYPE *)__buffer_##NAME };

#define RINGBUFFER_APPEND(NAME, VALUE) \
	do {                                                              \
		NAME.items[(NAME.head + NAME.len) % NAME.capacity] = (VALUE); \
		if (NAME.len < NAME.capacity) ++NAME.len;                     \
	} while (0);

#define RINGBUFFER_CONSUME(NAME) \
	NAME.items[NAME.head];                           \
	do {                                             \
		NAME.items[NAME.head] = 0;                   \
		NAME.head = (NAME.head + 1) % NAME.capacity; \
		--NAME.len;                                  \
	} while (0)

// math
int point_in_rect(float px, float py, float x, float y, float w, float h);
int nearest_pow2(int value);
// angle in radians, segments are clockwise.
float calculate_angle_segment(float angle, int segments);

// coordinate systems
vec2s world_to_screen(float vw, float vh, mat4 projection, mat4 view, mat4 model, vec3s point);
vec3s screen_to_world(float vw, float vh, mat4 projection, mat4 view, float screen_x, float screen_y);

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

