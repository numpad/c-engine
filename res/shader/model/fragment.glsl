precision highp float;

uniform sampler2D u_diffuse;

varying vec2 v_texcoord0;
varying vec3 v_normal;

void main() {
	vec4 diffuse = texture2D(u_diffuse, v_texcoord0);

	vec3 light_dir = normalize(vec3(0, 0, -5));
	float diff = max(dot(v_normal, -light_dir), 0.0);

	gl_FragColor = vec4(diffuse.rgb * diff, diffuse.a);
}

