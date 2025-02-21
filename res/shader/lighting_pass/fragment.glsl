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
uniform float u_lut_size;
uniform float u_z_near;
uniform float u_z_far;
uniform vec2 u_screen_resolution; // TODO: REMOVE
uniform float u_time;             // TODO: REMOVE

out vec4 Color;

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

vec3 srgb_to_linear(vec3 srgb)   { return pow(srgb,   vec3(      2.2)); }
vec4 srgb_to_linear(vec4 srgb)   { return pow(srgb,   vec4(      2.2)); }
vec3 linear_to_srgb(vec3 linear) { return pow(linear, vec3(1.0 / 2.2)); }
vec4 linear_to_srgb(vec4 linear) { return pow(linear, vec4(1.0 / 2.2)); }

vec4 with_outline(vec4 color) {
	int   size           = 2;
	vec3  outline_color  = 0.75 * vec3(0.14, 0.031, 0.059);

	vec2  pixel_step = 1.0 / vec2(u_screen_resolution.x, u_screen_resolution.y);
	vec4  position   = texture(u_position, v_texcoord);
	float depth      = (-position.z - u_z_near) / (u_z_far - u_z_near);

	float mx = 0.0;
	for (int y = -size; y <= size; ++y) {
		for (int x = -size; x <= size; ++x) {
			vec4 pos = texture(u_position, v_texcoord +  vec2(x, y) * pixel_step);
			float tempDepth = (-pos.z - u_z_near) / (u_z_far - u_z_near);
			mx = max(mx, (depth - tempDepth));
		}
	}
	if (mx > 0.02) {
		color.rbg *= outline_color;
	}
	return color;
}

vec4 with_colorgrading(vec4 color) {
	// TODO: remove split view, just for debug
	if (gl_FragCoord.x + abs(sin(gl_FragCoord.y * 0.25)) + sin(gl_FragCoord.y * 0.25 + u_time * 5.0) * 2.0 < u_screen_resolution.x * 0.5)
		return color;

	float grid_size = u_lut_size - 1.0;
	float lut_image_width = u_lut_size * u_lut_size;

	float u = floor(color.b * grid_size) / grid_size * (lut_image_width - grid_size);
	u = (floor(color.r * grid_size) / grid_size * grid_size) + u;
	u /= lut_image_width;
	float v = ceil(color.g * grid_size);
	v /= grid_size;
	v = 1.0 - v;

	vec3 left = texture(u_color_lut, vec2(u, v)).rgb;

	u = ceil(color.b * grid_size) / grid_size * (lut_image_width - grid_size);
	u = (ceil(color.r * grid_size) / grid_size *  grid_size) + u;
	u /= lut_image_width;
	v  = 1.0 - (ceil(color.g * grid_size) / grid_size);

	vec3 right = texture(u_color_lut, vec2(u, v)).rgb;

	color.r = mix(left.r, right.r, fract(color.r * grid_size));
	color.g = mix(left.g, right.g, fract(color.g * grid_size));
	color.b = mix(left.b, right.b, fract(color.b * grid_size));

	return color;
}

void main() {
	vec4 albedo   = texture(u_albedo,   v_texcoord);
	vec4 position = texture(u_position, v_texcoord);
	vec3 normal   = texture(u_normal,   v_texcoord).xyz;

	albedo = with_colorgrading(with_outline(albedo));
	albedo.rgb = linear_to_srgb(albedo.rgb);

	// Draw Depth
	//float depth = -position.z;
	//float normalized_depth = (depth - u_z_near) / (u_z_far - u_z_near);
	//Color = vec4(vec3(normalized_depth), albedo.a);

	// Draw final color
	Color = albedo;
}

