
precision mediump float;

uniform sampler2D u_texture;

varying vec2 v_texcoord;

void main() {
	vec4 color = texture2D(u_texture, v_texcoord * vec2(0.125, 0.25) + vec2(0.0, 0.75));
	gl_FragColor = color;
}

