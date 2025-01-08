#version 300 es
precision highp float;

uniform sampler2D u_diffuse;
uniform float u_highlight;

in vec2 v_texcoord0;
in vec3 v_normal;
in vec3 v_position;

layout(location=0) out vec4 Albedo;
layout(location=1) out vec4 Position;
layout(location=2) out vec4 Normal;

void main() {
	vec4 diffuse = texture(u_diffuse, v_texcoord0);
	if (int(u_highlight) == 1) {
		Albedo = vec4(1.0, 0.0, 0.0, diffuse.a);
	} else {
		Albedo = vec4(mix(diffuse.rgb, v_normal.xyz, 0.0), diffuse.a);
	}
	Position = vec4(v_position.xyz, 1.0);
	Normal = vec4(v_normal.xyz, 1.0);
}

