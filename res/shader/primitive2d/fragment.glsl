#version 300 es
precision mediump float;

uniform sampler2D u_texture;
uniform vec4 u_color_mult;
uniform vec4 u_color_add;
uniform vec2 u_texture_texelsize;
uniform vec4 u_subrect;

in vec2 v_texcoord;

out vec4 Color;

void main() {
	vec4 pixel = texture(u_texture, v_texcoord * u_subrect.zw + u_subrect.xy);

	Color = clamp(pixel * u_color_mult + u_color_add, vec4(0.0), vec4(1.0));
}

