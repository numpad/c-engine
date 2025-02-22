#version 300 es
precision mediump float;

uniform sampler2D u_texture;

in vec2 v_texcoord;
in vec4 v_color_mult;
in vec4 v_color_add;

out vec4 Color;

#define RENDER_NORMAL

void main() {
	vec4 pixel = vec4(0.0, 0.0, 0.0, 1.0);

#ifdef RENDER_SDF
	float dist = texture(u_texture, v_texcoord).a;

	float smoothing = 1.0/5.0;
	float border = 0.5;
	float alpha = smoothstep(border - smoothing, border + smoothing, dist);
	pixel = vec4(v_color_add.rgb, alpha);
#endif

#ifdef RENDER_NORMAL
	pixel.rgb = v_color_add.rgb;
	pixel.a = texture(u_texture, v_texcoord).a;
#endif

	Color = pixel;
}

