precision highp float;

uniform sampler2D u_diffuse;

varying vec2 v_texcoord0;
varying vec3 v_normal;

void main() {
	vec4 diffuse = texture2D(u_diffuse, v_texcoord0);

	gl_FragColor = vec4(mix(diffuse.rgb, v_normal.xyz, 0.2), diffuse.a);
}

