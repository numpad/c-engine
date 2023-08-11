
uniform sampler2D u_texture;
uniform vec2 u_tilepos;
uniform vec2 u_tilesize;

varying vec2 v_texcoord;

void main() {
	vec4 color = texture2D(u_texture, (v_texcoord + u_tilepos) * u_tilesize);
	gl_FragColor = color;
}

