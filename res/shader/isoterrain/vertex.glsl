
precision mediump float;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform vec3 u_pos;

attribute vec2 a_position;
attribute vec2 a_texcoord;

varying vec2 v_texcoord;

void main() {
	v_texcoord = a_texcoord;
	gl_Position = u_projection * u_view * vec4(a_position + u_pos.xy, u_pos.z, 1.0);
}

