precision mediump float;

varying vec2 v_texcoord0;
varying vec3 v_normal;

void main() {
	vec4 pixel = vec4(v_normal.xyz, 1.0);
	gl_FragColor = pixel;
}

