#version 300 es
precision mediump float;

uniform sampler2D u_texture;

in vec2 v_texcoord;
in vec4 v_color_mult;
in vec4 v_color_add;

out vec4 Color;

void main() {
	vec4 pixel = texture(u_texture, v_texcoord);
	if (pixel.a < 0.1) {
		discard;
	}
	pixel = pixel * v_color_mult + v_color_add;

	//Color = clamp(pixel * u_color_mult + u_color_add, vec4(0.0), vec4(1.0));
	Color = clamp(pixel, vec4(0.0), vec4(1.0));
}

