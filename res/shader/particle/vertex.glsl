#version 300 es
precision mediump float;

uniform mat4 u_projection;
uniform mat4 u_view;

in vec2 POSITION;
in vec2 TEXCOORD;
in vec3 INSTANCE_POSITION;
in vec2 INSTANCE_SCALE;
in vec4 INSTANCE_COLOR;
in vec4 INSTANCE_TEXTURE_SUBRECT;

out vec2 v_texcoord;
out vec4 v_color;

void main() {
	// camera facing axis
	vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
	vec3 up    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

	vec3 quad_vertex_pos = (right * POSITION.x * INSTANCE_SCALE.x + up * POSITION.y * INSTANCE_SCALE.y);
	vec3 position = quad_vertex_pos + INSTANCE_POSITION;

	v_texcoord  = TEXCOORD * INSTANCE_TEXTURE_SUBRECT.zw + INSTANCE_TEXTURE_SUBRECT.xy;
	v_color     = INSTANCE_COLOR;
	gl_Position = u_projection * u_view * vec4(position, 1.0);
}

