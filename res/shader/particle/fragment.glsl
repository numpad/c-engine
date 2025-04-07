#version 300 es
precision mediump float;

uniform sampler2D u_texture;

in  vec2 v_texcoord;
in  vec4 v_color;
out vec4 Albedo;

void main() {
	vec4 color = texture(u_texture, v_texcoord);
	Albedo = v_color * color;
}

