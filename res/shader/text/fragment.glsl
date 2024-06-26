precision mediump float;

uniform sampler2D u_texture;

varying vec2 v_texcoord;
varying vec4 v_color_mult;
varying vec4 v_color_add;

#define RENDER_NORMAL

void main() {
	vec4 pixel = vec4(0.0, 0.0, 0.0, 1.0);

#ifdef RENDER_SDF
	float dist = texture2D(u_texture, v_texcoord).a;

	float smoothing = 1.0/5.0;
	float border = 0.5;
	float alpha = smoothstep(border - smoothing, border + smoothing, dist);
	pixel = vec4(v_color_add.rgb, alpha);
#endif

#ifdef RENDER_NORMAL
	pixel.rgb = v_color_add.rgb;
	pixel.a = texture2D(u_texture, v_texcoord).a;
#endif

	gl_FragColor = pixel;
}

