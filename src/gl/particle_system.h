#ifndef CENGINE_PARTICLES_H
#define CENGINE_PARTICLES_H

#include "gl/opengles3.h"
#include "gl/shader.h"
#include <cglm/cglm.h>

// TODO: split into particle_system & particle_renderer
//       renderer holds resources & draws systems,
//       system holds particles & render modes (blend func, ...)
// TODO: bonus points if we manage to hold all particles
//       in a single VBO - needs profiling to see if it actually
//       makes sense tho.

typedef struct particle_renderer particle_renderer_t;

struct particle_instance_data {
	vec3 pos;
	vec2 scale;
	// vec4 texcoord;
};

struct particle_renderer {
	GLuint   vao;
	GLuint   vbo;
	shader_t shader;
	// per system data
	GLuint instance_vbo;
	usize  particles_count;
	usize  particles_capacity;
	// TODO: SoA to split up GPU & CPU particle data?
	struct particle_instance_data *particle_instance_data;
};

void  particle_renderer_init   (struct particle_renderer *);
void  particle_renderer_destroy(struct particle_renderer *);
void  particle_renderer_draw   (struct particle_renderer *, struct camera *);
usize particle_renderer_spawn  (struct particle_renderer *);

#endif

