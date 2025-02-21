#version 300 es
precision highp float;

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
in vec4 JOINTS_0; // TODO: ivec?
in vec4 WEIGHTS_0;

out vec2 v_texcoord0;
out vec3 v_normal;
out vec3 v_world_position;
out vec3 v_view_position;

void main() {
	vec4 total_position = vec4(0.0);
	vec4 total_normal = vec4(0.0);
	for (int i = 0; i < MAX_WEIGHTS; ++i) {
		vec4 local_position = u_bone_transforms[int(JOINTS_0[i])] * vec4(POSITION, 1.0);
		total_position += local_position * WEIGHTS_0[i];

		vec4 world_normal = u_bone_transforms[int(JOINTS_0[i])] * vec4(NORMAL, 0.0);
		total_normal += world_normal * WEIGHTS_0[i];
	}

	v_texcoord0 = TEXCOORD_0;
	v_normal = normalize(u_normalMatrix * NORMAL);

	v_world_position = (u_model * vec4(POSITION, 1.0)).xyz;
	v_view_position = (u_view * u_model * vec4(POSITION, 1.0)).xyz;
	
	if (int(u_is_rigged) == 1) {
		gl_Position = u_projection * u_view * u_model * total_position;
	} else {
		gl_Position = u_projection * u_view * u_model * vec4(POSITION, 1.0);
	}
}

