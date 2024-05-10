precision mediump float;

uniform mat4 u_mvp;

attribute vec3 POSITION;
attribute vec3 NORMAL;
attribute vec2 TEXCOORD_0;

varying vec2 v_texcoord0;
varying vec3 v_normal;

void main() {
	v_texcoord0 = TEXCOORD_0;
	v_normal = NORMAL;

	gl_Position = u_mvp * vec4(POSITION, 1.0);
}

