#version 300 es
precision highp float;

in vec2 v_texcoord;

// Buffers
uniform sampler2D u_albedo;
uniform sampler2D u_position;
uniform sampler2D u_normal;

// State
uniform vec2 u_screen_resolution;
uniform float u_time;

out vec4 Color;

float campfire_flicker(float time) {
	float noise = fract(sin(dot(vec2(time, time * 1.3), vec2(12.9898, 78.233))) * 43758.5453);
	float flicker = 0.7 + 0.3 * sin(time * 6.0 + noise * 5.0);
	return clamp(flicker, 0.0, 1.0);
}

float campfire_flicker_soft(float time) {
	float smoothNoise = fract(sin(dot(vec2(time * 0.7, time * 1.1), vec2(12.9898, 78.233))) * 43758.5453);
	smoothNoise += fract(sin(dot(vec2(time * 1.3, time * 0.9), vec2(63.7264, 10.873))) * 43758.5453) * 0.5;
	smoothNoise = smoothstep(0.0, 1.0, smoothNoise); // Smooth the noise

	float flicker = 0.85 + 0.15 * sin(time * 4.0) * smoothNoise;
	return clamp(flicker, 0.3, 1.0);
}

float map_steps(float num, int steps) {
	float stepSize = 1.0 / float(steps);
	return floor(num / stepSize + 0.5) * stepSize;
}

float map_steps_soft(float num, int steps) {
	float stepSize = 1.0 / float(steps);
	float step = floor(num / stepSize);
	float fraction = mod(num, stepSize) / stepSize;
	float smoothFraction = smoothstep(0.5, 0.525, fraction);
	return (step + smoothFraction) * stepSize;
}

void main() {
	vec4 albedo = texture(u_albedo, v_texcoord);
	vec3 position = texture(u_position, v_texcoord).xyz;
	vec3 normal = texture(u_normal, v_texcoord).xyz;

	vec4 out_color = albedo;

	// Some experiment:
	float t = campfire_flicker_soft(u_time * 5.0) * 0.7 + campfire_flicker(u_time) * 0.3;
	float l = length(position - vec3(100.0, 0.0, 550.0));
	float lf = smoothstep(0.0, 1.0, 1.0 - (l / (600.0 + t * 80.0f)));
	out_color.rgb *= vec3(0.5, 0.5, 0.65) * clamp(lf, 0.5, 1.0);
	out_color.r += lf*lf * 0.175;

	Color = out_color;
}

