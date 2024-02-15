precision mediump float;

uniform sampler2D u_texture;
uniform vec4 u_color_mult;
uniform vec4 u_color_add;
uniform vec2 u_texture_texelsize;
uniform vec4 u_subrect;

varying vec2 v_texcoord;

void main() {
	vec4 pixel = texture2D(u_texture, v_texcoord * u_subrect.zw + u_subrect.xy);

	gl_FragColor = clamp(pixel * u_color_mult + u_color_add, vec4(0.0), vec4(1.0));
}

