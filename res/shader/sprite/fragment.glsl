precision mediump float;

uniform sampler2D u_texture;

varying vec2 v_texcoord;
varying vec4 v_color_mult;
varying vec4 v_color_add;

void main() {
	vec4 pixel = texture2D(u_texture, v_texcoord);
	if (pixel.a < 0.1) {
		discard;
	}
	pixel = pixel * v_color_mult + v_color_add;

	//gl_FragColor = clamp(pixel * u_color_mult + u_color_add, vec4(0.0), vec4(1.0));
	gl_FragColor = clamp(pixel, vec4(0.0), vec4(1.0));
}

