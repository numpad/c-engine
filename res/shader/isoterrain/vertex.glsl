
precision mediump float;

uniform mat4 u_projection;
uniform mat4 u_view;

attribute vec3 a_pos;
attribute vec2 a_texcoord;
attribute vec4 a_color_mult;
attribute vec4 a_color_add;

varying vec2 v_texcoord;

void main() {
	v_texcoord = a_texcoord;
	gl_Position = u_projection * u_view * vec4(a_pos, 1.0);
}

