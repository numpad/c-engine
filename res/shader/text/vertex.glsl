precision mediump float;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

attribute vec3 a_pos;
attribute vec2 a_texcoord;
attribute vec4 a_color_mult;
attribute vec4 a_color_add;

varying vec2 v_texcoord;
varying vec4 v_color_mult;
varying vec4 v_color_add;

void main() {
	v_texcoord = a_texcoord;
	v_color_mult = a_color_mult;
	v_color_add = a_color_add;

	gl_Position = u_projection * u_view * u_model * vec4(a_pos, 1.0);
}

