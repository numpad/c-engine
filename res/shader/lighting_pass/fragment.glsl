#version 300 es
precision highp float;

in vec2 v_texcoord;

uniform sampler2D u_albedo;
uniform sampler2D u_position;
uniform sampler2D u_normal;

out vec4 Color;

void main() {
	vec4 albedo = texture(u_albedo, v_texcoord);
	vec4 position = texture(u_position, v_texcoord);
	vec4 normal = texture(u_normal, v_texcoord);

	Color = vec4(albedo.rgb, albedo.a);
}

