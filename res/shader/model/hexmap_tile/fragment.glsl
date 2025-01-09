#version 300 es
precision mediump float;

layout(std140) uniform Global {
	float time;
};

uniform sampler2D u_diffuse;
uniform float u_highlight;

in vec2 v_texcoord0;
in vec3 v_normal;
in vec3 v_local_position;
in vec3 v_world_position;

layout(location=0) out vec4 Albedo;
layout(location=1) out vec4 Position;
layout(location=2) out vec4 Normal;

float sdCircle(vec2 p, float r) {
	return length(p)-r;
}
vec3 sdgHexagon(vec2 p, float r) {
	const vec3 k = vec3(-0.866025404,0.5,0.577350269);
	vec2 s = sign(p); p = abs(p);
	float w = dot(k.xy,p);
	p -= 2.0*min(w,0.0)*k.xy;
	p -= vec2(clamp(p.x, -k.z*r, k.z*r), r);
	float d = length(p)*sign(p.y);
	vec2  g = (w<0.0) ? mat2(-k.y,-k.x,-k.x,k.y)*p : p;
	return vec3(d, s*g/d);
}

vec3 highlight_walkable_area(vec3 local_pos) {
	vec2 p = vec2(-local_pos.z, local_pos.x); // rotate by 90° because pointy-top hexagons.
	// TODO: Floating point inaccuarcies with "time", might look bad after some time.
	float d = sdgHexagon(p, 0.5 + time * 0.1).x;
	vec3 col = vec3(0.9,0.6,0.3);
	col *= 0.9 + 0.1*cos(50.0*d);
	return col;
}

void main() {
	vec4 diffuse = texture(u_diffuse, v_texcoord0);
	if (int(u_highlight) == 1) {
		Albedo = vec4(mix(vec3(1.0, 0.0, 0.0), diffuse.rgb, 0.5), diffuse.a);
	} else if (int(u_highlight) == 2) {
		Albedo = vec4(highlight_walkable_area(v_local_position), diffuse.a);
	} else {
		Albedo = diffuse.rgba;
	}
	Position = vec4(v_world_position.xyz, 1.0);
	Normal = vec4(v_normal.xyz, 1.0);
}

