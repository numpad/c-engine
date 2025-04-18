#ifndef PARTICLE_SPAWNERS_H
#define PARTICLE_SPAWNERS_H

#include <cglm/types.h>
#include "gl/particle_system.h"

void particle_spawn_flame      (struct particle_renderer *renderer, vec3 pos);
void particle_spawn_gain_health(struct particle_renderer *renderer, vec3 pos);

#endif

