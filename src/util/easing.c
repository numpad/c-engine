#include "util/easing.h"
#include <math.h>

// sine
float ease_sine_in(float n) {
	return 1.0f - cosf((n * 3.141592653f) * 0.5f);
}

float ease_sine_out(float n) {
	return sinf((n * 3.141592653f) * 0.5f);
}

float ease_sine_inout(float n) {
	return -(cosf(n * 3.141592653f) - 1.0f) * 0.5f;
}


// quad
float ease_quad_in(float n) {
	return n * n;
}

float ease_quad_out(float n) {
	return 1.0f - (1.0f - n) * (1.0f - n);
}

float ease_quad_inout(float n) {
	return n < 0.5f ? 2.0f * n * n : 1.0f - powf(-2.0f * n + 2.0f, 2.0f) * 0.5f;
}


// cubic
float ease_cubic_in(float n) {
	return n * n * n;
}

float ease_cubic_out(float n) {
	return 1.0f - powf(1.0f - n, 3.0f);
}

float ease_cubic_inout(float n) {
	return n < 0.5f ? 4.0f * n * n * n : 1.0f - powf(-2.0f * n + 2.0f, 3.0f) * 0.5f;
}


// quart
float ease_quart_in(float n) {
	return n * n * n * n;
}

float ease_quart_out(float n) {
	return 1.0f - powf(1.0f - n, 4.0f);
}

float ease_quart_inout(float n) {
	return n < 0.5f ? 8.0f * n * n * n * n : 1.0f - pow(-2.0f * n + 2.0f, 4.0f) * 0.5f;
}

// expo
float ease_expo_in(float n) {
	return n <= 0.0f ? 0.0f : powf(2.0f, 10.0f * n - 10.0f);
}

float ease_expo_out(float n) {
	return n >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * n);
}

float ease_expo_inout(float n) {
	return n <= 0.0f
		? 0.0f
		: n >= 1.0f
		? 1.0f
		: n < 0.5f ? powf(2.0f, 20.0f * n - 10.0f) * 0.5f
		: (2.0f - powf(2.0f, -20.0f * n + 10.0f)) * 0.5f;
}


// back
float ease_back_in(float n) {
	return 2.70158f * n * n * n - 1.70158f * n * n;
}

float ease_back_out(float n) {
	return 1.0f + 2.70158f * powf(n - 1.0f, 3.0f) + 1.70158f * powf(n - 1.0f, 2.0f);
}

float ease_back_inout(float n) {
	const float c1 = 1.70158;
	const float c2 = c1 * 1.525;

	return n < 0.5f
		? (powf(2.0f * n, 2.0f) * ((c2 + 1.0f) * 2.0f * n - c2)) * 0.5f
		: (powf(2.0f * n - 2.0f, 2.0f) * ((c2 + 1.0f) * (n * 2.0f - 2.0f) + c2) + 2.0f) * 0.5f;
}

