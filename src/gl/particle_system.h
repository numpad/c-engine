#ifndef CENGINE_PARTICLES_H
#define CENGINE_PARTICLES_H

#include <cglm/cglm.h>
#include "gl/opengles3.h"
#include "gl/shader.h"
#include "gl/texture.h"

// TODO: split into particle_system & particle_renderer
//       renderer holds resources & draws systems,
//       system holds particles & render modes (blend func, ...)
// TODO: bonus points if we manage to hold all particles
//       in a single VBO - needs profiling to see if it actually
//       makes sense tho.

typedef struct particle_renderer particle_renderer_t;

enum particle_flag {
	PARTICLE_FLAGS_NONE            = 0,
	PARTICLE_FLAGS_SHRINK_ON_DEATH = 1,
	PARTICLE_FLAGS_FADE_TO_BLACK   = 2,
};

// TODO: SoA this? Scale & Subrect will rarely change, however Pos will change
//       often. Can we use this? Upload pos each frame, scale&subrect only on
//       spawn/change?
struct particle_render_data {
	float pos[3];
	float __padding0[1];
	float scale[2];
	float __padding1[2];
	float color[4];
	float texture_subrect[4];
};

struct particle_data {
	vec3  velocity;
	vec3  acceleration;
	float lifetime;
	float mass;
	vec3  force_acc;
	vec3  gravity; // TODO: maybe move this to system?
	vec3  drag;
	u64   flags;
};

struct particle_renderer {
	GLuint    vao;
	GLuint    vbo;
	shader_t  shader;
	texture_t texture;
	// per system data
	GLuint instance_vbo;
	usize  particles_count;
	usize  particles_capacity;
	// TODO: SoA to split up GPU & CPU particle data?
	struct particle_render_data *particle_render_data;
	struct particle_data *particle_data;
};

void  particle_renderer_init   (struct particle_renderer *);
void  particle_renderer_destroy(struct particle_renderer *);
void  particle_renderer_draw   (struct particle_renderer *, struct camera *);

// System specific
usize particle_renderer_spawn  (struct particle_renderer *, vec3 pos);
void  particle_renderer_update (struct particle_renderer *, float dt);

void particle_renderer_set_particle_texture_subrect(struct particle_renderer *, usize particle_index, int x, int y, int w, int h);

#endif

