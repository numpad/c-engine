#version 300 es
precision highp float;

uniform sampler2D u_texture;

in vec2 v_texcoord;
out vec4 Color;

void main() {
	//p = floor(p) + min(fract(p) / fwidth(p), 1.0) - 0.5;
	vec4 color = texture(u_texture, v_texcoord);
	Color = color;
}

