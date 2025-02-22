#version 300 es
precision highp float;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat3 u_normalMatrix;

in vec3 POSITION;
in vec3 NORMAL;
in vec2 TEXCOORD_0;

out vec2 v_texcoord0;
out vec3 v_normal;
out vec3 v_local_position;
out vec3 v_world_position;
out vec3 v_view_position;

void main() {
	v_texcoord0 = TEXCOORD_0;
	v_normal = normalize(u_normalMatrix * NORMAL);
	v_local_position = POSITION;
	v_world_position = (u_model * vec4(POSITION, 1.0)).xyz;
	v_view_position = (u_view * u_model * vec4(POSITION, 1.0)).xyz;

	gl_Position = u_projection * u_view * u_model * vec4(POSITION, 1.0);
}

