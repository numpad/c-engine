#version 300 es
precision mediump float;

uniform mat4 u_projection;
uniform mat4 u_view;

in vec3 a_pos;
in vec2 a_texcoord;
in vec4 a_color_mult;
in vec4 a_color_add;

out vec2 v_texcoord;
out vec4 v_color_mult;
out vec4 v_color_add;

void main() {
	v_texcoord = a_texcoord;
	v_color_mult = a_color_mult;
	v_color_add = a_color_add;

	gl_Position = u_projection * u_view * vec4(a_pos, 1.0);
}

