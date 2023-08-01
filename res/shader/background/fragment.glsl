
uniform sampler2D u_texture;

void main() {
	gl_FragColor = vec4(texture2D(u_texture, gl_FragCoord.xy / 1024.0).rgb, 1.0);
}

