#include "game/particle_spawners.h"
#include "gl/particle_system.h"
#include <cglm/types.h>
#include <math.h>

#include <cglm/vec3.h>

void particle_spawn_flame(struct particle_renderer *renderer, vec3 pos) {
	usize i = particle_renderer_spawn(renderer, pos);
	struct particle_render_data *draw = &renderer->particle_render_data[i];
	struct particle_data *particle = &renderer->particle_data[i];
	switch (rand() % 2) {
	case 0: glm_vec3_copy((vec3){0.7f, 0.5f, 0.05f}, draw->color);
		break;
	case 1: glm_vec3_copy((vec3){0.7f, 0.2f, 0.04f}, draw->color);
		break;
	}
	draw->color[3] = 0.2f + rng_f() * 0.4f;
	const float rx = (rng_f() * 2.0f - 1.0f);
	const float rz = (rng_f() * 2.0f - 1.0f);
	draw->pos[0] += rx * 0.35f;
	draw->pos[1] += 1.0f;
	draw->pos[2] += rz * 0.35f;
	glm_vec2_scale(draw->scale, 0.75f + rng_f() * 0.5f, draw->scale);
	particle->lifetime = 1.9f + rng_f() * 0.65f;
	particle->gravity[1] = 0.1f + rng_f() * 0.1f;
	particle->velocity[0] += rx * 0.45f;
	particle->velocity[1] = -0.2f;
	particle->velocity[2] += rz * 0.45f;
	particle->acceleration[1] = 0.0f;
	particle->flags = PARTICLE_FLAGS_SHRINK_ON_DEATH;
}

