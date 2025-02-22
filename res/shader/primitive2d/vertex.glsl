#version 300 es
precision mediump float;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

in vec2 a_position;
in vec2 a_texcoord;

out vec2 v_texcoord;

void main() {
	v_texcoord = a_texcoord;
	gl_Position = u_projection * u_view * u_model * vec4(a_position, 0.0, 1.0);
}

