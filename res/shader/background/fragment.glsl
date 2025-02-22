#version 300 es
precision mediump float;

uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform vec3 u_parallax_offset;

layout(location=0) out vec4 Albedo;
layout(location=1) out vec4 Position;
layout(location=2) out vec4 Normal;

void main() {
	vec2 texcoord = gl_FragCoord.xy / u_resolution.y + u_parallax_offset.xy;
	vec4 pixel = texture(u_texture, texcoord);
	pixel.rgb = pow(pixel.rgb, vec3(2.2));
	Albedo = pixel;
	Position = vec4(0.0);
	Normal = vec4(0.0);
}

