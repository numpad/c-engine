#version 300 es
precision mediump float;

#define MAX_BONES 48
#define MAX_WEIGHTS 4

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat3 u_normalMatrix;
uniform mat4 u_bone_transforms[MAX_BONES];
uniform float u_is_rigged;

in vec3 POSITION;
in vec3 NORMAL;
in vec2 TEXCOORD_0;
in vec4 JOINTS_0;
in vec4 WEIGHTS_0;

out vec2 v_texcoord0;
out vec3 v_normal;
out vec3 v_world_position;
out vec3 v_view_position;

void main() {
	v_texcoord0 = TEXCOORD_0;
	// TODO: fix normals
	v_normal = normalize(u_normalMatrix * NORMAL);

	if (u_is_rigged > 0.5) {
		mat4 skin_matrix =
			WEIGHTS_0[0] * u_bone_transforms[int(JOINTS_0[0])] +
			WEIGHTS_0[1] * u_bone_transforms[int(JOINTS_0[1])] +
			WEIGHTS_0[2] * u_bone_transforms[int(JOINTS_0[2])] +
			WEIGHTS_0[3] * u_bone_transforms[int(JOINTS_0[3])];

		vec4 total_position = skin_matrix * vec4(POSITION, 1.0);
		v_world_position = (u_model * total_position).xyz;
		v_view_position = (u_view * u_model * total_position).xyz;
		gl_Position = u_projection * u_view * u_model * total_position;
	} else {
		v_world_position = (u_model * vec4(POSITION, 1.0)).xyz;
		v_view_position = (u_view * u_model * vec4(POSITION, 1.0)).xyz;
		gl_Position = u_projection * u_view * u_model * vec4(POSITION, 1.0);
	}
}

