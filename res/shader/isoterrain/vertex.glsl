#version 300 es
precision highp float;

uniform mat4 u_projection;
uniform mat4 u_view;

in vec3 a_pos;
in vec2 a_texcoord;
in vec4 a_color_mult;
in vec4 a_color_add;

out vec2 v_texcoord;

void main() {
	v_texcoord = a_texcoord;
	gl_Position = u_projection * u_view * vec4(a_pos, 1.0);
}

