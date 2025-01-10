#version 300 es
precision mediump float;

#define PI 3.141592654

layout(std140) uniform Global {
	float time;
};

uniform sampler2D u_diffuse;
uniform float u_highlight;
uniform vec3 u_player_world_pos;

in vec2 v_texcoord0;
in vec3 v_normal;
in vec3 v_local_position;
in vec3 v_world_position;

layout(location=0) out vec4 Albedo;
layout(location=1) out vec4 Position;
layout(location=2) out vec4 Normal;

// SDFs
//
vec3 sdgCircle(in vec2 p, in float r) {
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
// Simplex 2D noise
//
vec3 permute(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }
float snoise(vec2 v) {
	const vec4 C = vec4(0.211324865405187, 0.366025403784439,
			-0.577350269189626, 0.024390243902439);
	vec2 i  = floor(v + dot(v, C.yy) );
	vec2 x0 = v -   i + dot(i, C.xx);
	vec2 i1;
	i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
	vec4 x12 = x0.xyxy + C.xxzz;
	x12.xy -= i1;
	i = mod(i, 289.0);
	vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
			+ i.x + vec3(0.0, i1.x, 1.0 ));
	vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy),
				dot(x12.zw,x12.zw)), 0.0);
	m = m*m ;
	m = m*m ;
	vec3 x = 2.0 * fract(p * C.www) - 1.0;
	vec3 h = abs(x) - 0.5;
	vec3 ox = floor(x + 0.5);
	vec3 a0 = x - ox;
	m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
	vec3 g;
	g.x  = a0.x  * x0.x  + h.x  * x0.y;
	g.yz = a0.yz * x12.xz + h.yz * x12.yw;
	return 130.0 * dot(m, g);
}

vec3 highlight_walkable_area(vec3 diffuse, vec2 p) {
	p = vec2(-p.y, p.x); // rotate by 90° because pointy-top hexagons.
	// TODO: Floating point inaccuarcies with "time", might look bad after some time.
	float t = mod(time, PI);
	p += snoise(p) * 0.05 + snoise(diffuse.xy) * 0.025;;
	float f = abs(sin(t * 2.0) * 0.5);
	float d = sdgHexagon(p, 2.0 + (t + f) * 0.1).x;
	float alpha = max(sin(20.0 * d) * f, 0.1);
	vec3 blue = vec3(0.21, 0.24, 0.95);
	return mix(diffuse, blue, alpha);
}

void main() {
	vec4 diffuse = texture(u_diffuse, v_texcoord0);
	if (int(u_highlight) == 1) {
		Albedo = vec4(mix(vec3(1.0, 0.0, 0.0), diffuse.rgb, 0.5), diffuse.a);
	} else if (int(u_highlight) == 2) {
		vec2 p = v_world_position.xz - u_player_world_pos.xz;
		p *= 0.01;

		vec3 effect = highlight_walkable_area(diffuse.rgb, p);
		Albedo = vec4(effect, diffuse.a);
	} else {
		Albedo = diffuse.rgba;
	}
	Position = vec4(v_world_position.xyz, 1.0);
	Normal = vec4(v_normal.xyz, 1.0);
}

