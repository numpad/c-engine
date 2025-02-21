#version 300 es
precision mediump float;

#define PI 3.141592654

layout(std140) uniform Global {
	float periodic_time;
	vec4  display_resolution;
};

uniform sampler2D u_diffuse;
uniform float u_highlight;
uniform vec3 u_player_world_pos;

in vec2 v_texcoord0;
in vec3 v_normal;
in vec3 v_local_position;
in vec3 v_world_position;
in vec3 v_view_position;

layout(location=0) out vec4 Albedo;
layout(location=1) out vec4 Position;
layout(location=2) out vec4 Normal;

// SDFs
//
vec3 sdgCircle(vec2 p, float r) {
	float d = length(p);
	return vec3( d-r, p/d );
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

vec3 highlight_enemy_tile(vec3 diffuse, vec2 p) {
	p = vec2(-p.y, p.x); // rotate by 90° because pointy-top hexagons.
	vec3 highlight_color = vec3(0.65, 0.0, 0.0);
	float hex = sdgHexagon(p, 0.5).x;
	float border = smoothstep(0.1, 0.8, hex);
	float alpha = min(step(0.4, border), 0.15) + clamp(smoothstep(0.2, 0.6, hex), 0.1, 0.5);
	return mix(diffuse, highlight_color, alpha);
}
vec3 highlight_walkable_area(vec3 diffuse, vec2 p) {
	p = vec2(-p.y, p.x); // rotate by 90° because pointy-top hexagons.
	vec3 highlight_color = vec3(1.0); // blue: vec3(0.31, 0.31, 0.77);
	// TODO: Floating point inaccuarcies with "time", might look bad after some time.
	// TODO: This only works on mobile with highp?
	//       float t = mod(time * 0.3, 1.0 + 1e8); // small epsilon to prevent jumps?
	float t = periodic_time;

	float circle = sdgCircle(p, t).x;
	float fast_circle = sdgCircle(p, t * 10.0).x;
	float ripples = max(cos(20.0 * circle), 0.0);
	float wave = max(cos(1.0 * fast_circle), 0.0);
	float alpha = ripples * wave * 0.3 + 0.2;
	return mix(diffuse, highlight_color, alpha);
}

void main() {
	vec4 diffuse = texture(u_diffuse, v_texcoord0);
	diffuse.rgb = pow(diffuse.rgb, vec3(2.2));
	if (int(u_highlight) == 1) {
		vec3 effect = highlight_enemy_tile(diffuse.rgb, v_local_position.xz);
		Albedo = vec4(effect, diffuse.a);
	} else if (int(u_highlight) == 2) {
		vec2 p = v_world_position.xz - u_player_world_pos.xz;
		p *= 0.01;
		vec3 effect = highlight_walkable_area(diffuse.rgb, p);
		Albedo = vec4(effect, diffuse.a);
	} else {
		Albedo = diffuse.rgba;
	}
	Position = vec4(v_view_position.xyz, 1.0);
	Normal = vec4(v_normal.xyz, 1.0);
}

