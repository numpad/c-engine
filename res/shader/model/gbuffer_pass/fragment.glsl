#version 300 es
precision highp float;

uniform sampler2D u_diffuse;

in vec2 v_texcoord0;
in vec3 v_normal;
in vec3 v_world_position;
in vec3 v_view_position;

layout(location=0) out vec4 Albedo;
layout(location=1) out vec4 Position;
layout(location=2) out vec4 Normal;

void main() {
	vec4 diffuse = texture(u_diffuse, v_texcoord0);
	diffuse.rgb = pow(diffuse.rgb, vec3(2.2));
	Albedo = vec4(mix(diffuse.rgb, v_normal.xyz, 0.0), diffuse.a);
	Position = vec4(v_view_position.xyz, 1.0);
	Normal = vec4(v_normal.xyz, 1.0);
}

