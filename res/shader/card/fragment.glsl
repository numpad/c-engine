
precision mediump float;

uniform sampler2D u_texture;
uniform vec4 u_cardwindow_offset;

varying vec2 v_texcoord;

void main() {
	// get pixel of card border
	vec4 color = texture2D(u_texture, v_texcoord * vec2(0.125, 0.125) + vec2(0.0, 0.75));

	// replace card-window
	if (color.a > 0.9 && color.a < 1.0) {
		color = texture2D(u_texture,
			(color.xy) * u_cardwindow_offset.zw + u_cardwindow_offset.xy);
	}

	gl_FragColor = color;
}

