precision mediump float;

uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform float u_parallax_offset;

void main() {
	gl_FragColor = texture2D(u_texture, gl_FragCoord.xy / u_resolution.y + vec2(u_parallax_offset, 0.0));
}

