precision mediump float;

uniform sampler2D u_texture;
uniform float u_parallax_offset;

void main() {
	gl_FragColor = texture2D(u_texture, gl_FragCoord.xy / 1024.0 + vec2(u_parallax_offset, 0.0));
}

