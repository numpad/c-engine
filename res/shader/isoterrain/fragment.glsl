
precision mediump float;

uniform sampler2D u_texture;

varying vec2 v_texcoord;

void main() {
	//p = floor(p) + min(fract(p) / fwidth(p), 1.0) - 0.5;

	vec4 color = texture2D(u_texture, v_texcoord);
	gl_FragColor = color;
}

