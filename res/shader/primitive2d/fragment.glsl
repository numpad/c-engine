precision mediump float;

uniform sampler2D u_texture;
uniform vec4 u_subrect;

varying vec2 v_texcoord;

void main() {
	vec4 pixel = texture2D(u_texture, v_texcoord * u_subrect.zw + u_subrect.xy);
	gl_FragColor = pixel;
}
