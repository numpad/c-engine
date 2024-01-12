precision mediump float;

uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform vec3 u_parallax_offset;

void main() {
	vec2 p = gl_FragCoord.xy / u_resolution.y + u_parallax_offset.xy;
	vec4 pixel = texture2D(u_texture, p);
	gl_FragColor = pixel;
}

