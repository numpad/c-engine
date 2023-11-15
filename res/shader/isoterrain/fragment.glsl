
precision mediump float;

uniform sampler2D u_texture;
uniform vec2 u_tilepos;
uniform vec2 u_tilesize;

varying vec2 v_texcoord;

void main() {
	vec2 p = (v_texcoord + u_tilepos) * u_tilesize;
	//p = floor(p) + min(fract(p) / fwidth(p), 1.0) - 0.5;

	vec4 color = texture2D(u_texture, p);
	gl_FragColor = color;
}

