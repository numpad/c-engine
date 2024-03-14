#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>


typedef unsigned int uint;
typedef size_t usize;

// defines
#define UTIL_FOR(_i, max) for (size_t _i = 0; _i < max; ++_i)
#define count_of(arr) (sizeof(arr) / sizeof(arr[0]))

// math
int point_in_rect(float px, float py, float x, float y, float w, float h);
int nearest_pow2(int value);
// angle in radians, segments are clockwise.
float calculate_angle_segment(float angle, int segments);

// string utils
const char *str_match_bracket(const char *str, size_t len, char open, char close);

#endif

