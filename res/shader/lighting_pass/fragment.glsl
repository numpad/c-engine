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

void main() {
	vec4 albedo = texture(u_albedo, v_texcoord);
	vec3 position = texture(u_position, v_texcoord).xyz;
	vec3 normal = texture(u_normal, v_texcoord).xyz;

	vec4 out_color = albedo;

	Color = out_color;
}

