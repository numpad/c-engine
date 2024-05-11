precision highp float;

uniform mat4 u_mvp;
uniform mat4 u_modelView;
uniform mat3 u_normalMatrix;

attribute vec3 POSITION;
attribute vec3 NORMAL;
attribute vec2 TEXCOORD_0;

varying vec2 v_texcoord0;
varying vec3 v_normal;

void main() {
	v_texcoord0 = TEXCOORD_0;
	v_normal = normalize(u_normalMatrix * NORMAL);

	gl_Position = u_mvp * vec4(POSITION, 1.0);
}

