precision mediump float;

attribute vec2 a_position;
attribute vec2 a_texcoord;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

varying vec2 v_texcoord;

void main() {
	v_texcoord = a_texcoord;
	gl_Position = u_projection * u_view * u_model * vec4(a_position, 0.0, 1.0);
}

