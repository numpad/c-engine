#version 300 es
precision highp float;

in vec2 v_texcoord;

// Globals
layout(std140) uniform Global {
	float periodic_time;
	vec4  display_resolution;
};


// Buffers
uniform sampler2D u_albedo;
uniform sampler2D u_position;
uniform sampler2D u_normal;
uniform sampler2D u_color_lut;

// State
uniform vec2 u_screen_resolution; // TODO: REMOVE
uniform float u_time;             // TODO: REMOVE

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

vec4 srgb_to_linear(vec4 srgb)   { return pow(srgb,   vec4(      2.2)); }
vec4 linear_to_srgb(vec4 linear) { return pow(linear, vec4(1.0 / 2.2)); }

vec4 with_outline(vec4 color) {
	vec2 pixel_step = 1.0 / vec2(u_screen_resolution.x, u_screen_resolution.y);
	vec4 albedo_l = texture(u_albedo, v_texcoord + vec2(-2.0,  0.0) * pixel_step);
	vec4 albedo_r = texture(u_albedo, v_texcoord + vec2( 2.0,  0.0) * pixel_step);
	vec4 albedo_t = texture(u_albedo, v_texcoord + vec2( 0.0, -2.0) * pixel_step);
	vec4 albedo_b = texture(u_albedo, v_texcoord + vec2( 0.0,  2.0) * pixel_step);
	if (color.a < 0.5 && (albedo_r.a > 0.5 || albedo_l.a > 0.5 || albedo_t.a > 0.5 || albedo_b.a > 0.5)) {
		return vec4(1.0, 0.0, 0.0, 1.0);
	}
	return color;
}

vec4 with_colorgrading(vec4 color) {
	// TODO: just for debug
	if (gl_FragCoord.x + abs(sin(gl_FragCoord.y * 0.25)) + sin(gl_FragCoord.y * 0.25 + u_time * 5.0) * 2.0 > u_screen_resolution.x * 0.5) return color;

	float grid_size = 15.0f;
	float u = floor(color.b * grid_size) / grid_size * (255.0 - grid_size);
	u = (floor(color.r * grid_size) / grid_size * grid_size) + u;
	u /= 255.0;
	float v = ceil(color.g * grid_size);
	v /= grid_size;
	v = 1.0 - v;

	vec3 left = texture(u_color_lut, vec2(u, v)).rgb;

	u = ceil(color.b * grid_size) / grid_size * (255.0 - grid_size);
	u = (ceil(color.r * grid_size) / grid_size *  grid_size) + u;
	u /= 255.0;
	v  = 1.0 - (ceil(color.g * grid_size) / grid_size);

	vec3 right = texture(u_color_lut, vec2(u, v)).rgb;

	color.r = mix(left.r, right.r, fract(color.r * grid_size));
	color.g = mix(left.g, right.g, fract(color.g * grid_size));
	color.b = mix(left.b, right.b, fract(color.b * grid_size));

	return color;
}

void main() {
	vec4 albedo = texture(u_albedo, v_texcoord);
	vec3 position = texture(u_position, v_texcoord).xyz;
	vec3 normal = texture(u_normal, v_texcoord).xyz;

	albedo.rgb = pow(albedo.rgb, vec3(2.2));
	albedo = with_colorgrading(with_outline(albedo));
	albedo.rgb = pow(albedo.rgb, vec3(1.0 / 2.2));

	// Some experiment:
	//float t = campfire_flicker_soft(u_time * 5.0) * 0.7 + campfire_flicker(u_time) * 0.3;
	//float l = length(position - vec3(690.0, 0.0, 690.0));
	//float lf = smoothstep(0.0, 1.0, 1.0 - (l / (600.0 + t * 80.0f)));
	//out_color.rgb *= vec3(0.5, 0.475, 0.725) * clamp(lf, 0.5, 1.0);
	//out_color.r += lf*lf * 0.175;

	Color = albedo;
}

