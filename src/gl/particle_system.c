#include "gl/particle_system.h"

#include <assert.h>
#include "gl/camera.h"
#include "gl/shader.h"

struct particle_vertex {
	float pos[2];
	float color[4];
};

static struct particle_vertex quad_vertices[] = {
		(struct particle_vertex){
			.pos[0]=-0.5f, .pos[1]=-0.5f,
			.color={ 0.0f, 1.0f, 0.0f, 1.0f }
		},
		(struct particle_vertex){
			.pos[0]= 0.5f, .pos[1]=-0.5f,
			.color={ 1.0f, 0.0f, 0.0f, 1.0f }
		},
		(struct particle_vertex){
			.pos[0]=-0.5f, .pos[1]= 0.5f,
			.color={ 0.0f, 0.0f, 1.0f, 1.0f }
		},
		(struct particle_vertex){
			.pos[0]= 0.5f, .pos[1]= 0.5f,
			.color={ 1.0f, 1.0f, 0.0f, 1.0f }
		}
	};

//
// Renderer specific
//

void particle_renderer_init(struct particle_renderer *system) {
	assert(system != NULL);

	// init system
	system->particles_count    =   0;
	system->particles_capacity = 2048; // TODO: reduce and implement growing buffer
	system->particle_render_data = malloc(system->particles_capacity * sizeof(struct particle_render_data));
	system->particle_data          = malloc(system->particles_capacity * sizeof(struct particle_data));
	glGenBuffers(1, &system->instance_vbo);

	// init renderer
	shader_init_from_dir(&system->shader, "res/shader/particle/");

	// vertex data
	glGenVertexArrays(1, &system->vao);
	glGenBuffers(1, &system->vbo);

	shader_use(&system->shader);
	glBindVertexArray(system->vao);
	glBindBuffer(GL_ARRAY_BUFFER, system->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, system->instance_vbo);
	glBufferData(GL_ARRAY_BUFFER, system->particles_capacity * sizeof(struct particle_render_data), NULL, GL_DYNAMIC_DRAW);

	// layout
	GLuint a_position          = glGetAttribLocation(system->shader.program, "POSITION");
	GLuint a_color             = glGetAttribLocation(system->shader.program, "COLOR");
	GLuint a_instance_position = glGetAttribLocation(system->shader.program, "INSTANCE_POSITION");
	GLuint a_instance_scale    = glGetAttribLocation(system->shader.program, "INSTANCE_SCALE");
	glBindBuffer(GL_ARRAY_BUFFER, system->vbo);
	gl_check glEnableVertexAttribArray(a_position);
	gl_check glEnableVertexAttribArray(a_color);
	assert(offsetof(struct particle_vertex, pos) == 0);
	assert(offsetof(struct particle_vertex, color) == 2 * sizeof(float));
	// base attribs
	gl_check glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE, sizeof(struct particle_vertex), (void*)offsetof(struct particle_vertex, pos));
	gl_check glVertexAttribPointer(a_color   , 4, GL_FLOAT, GL_FALSE, sizeof(struct particle_vertex), (void*)offsetof(struct particle_vertex, color));
	// instance attribs
	glBindBuffer(GL_ARRAY_BUFFER, system->instance_vbo);
	// attrib: instance position
	assert(offsetof(struct particle_render_data, pos) == 0 * sizeof(float));
	gl_check glEnableVertexAttribArray(a_instance_position);
	gl_check glVertexAttribPointer(a_instance_position, 3, GL_FLOAT, GL_FALSE, sizeof(struct particle_render_data), (void*)offsetof(struct particle_render_data, pos));
	glVertexAttribDivisor(a_instance_position, 1);
	// attrib: instance scale
	assert(offsetof(struct particle_render_data, scale) == 3 * sizeof(float));
	gl_check glEnableVertexAttribArray(a_instance_scale);
	gl_check glVertexAttribPointer(a_instance_scale, 2, GL_FLOAT, GL_FALSE, sizeof(struct particle_render_data), (void*)offsetof(struct particle_render_data, scale));
	glVertexAttribDivisor(a_instance_scale, 1);

	glBindVertexArray(0);
	shader_use(NULL);
}

void particle_renderer_destroy(struct particle_renderer *system) {
	glDeleteVertexArrays(1, &system->vao);
	glDeleteBuffers     (1, &system->vbo);
	glDeleteBuffers     (1, &system->instance_vbo);
	shader_destroy      (&system->shader);
	free(system->particle_render_data);
	free(system->particle_data);
}

void particle_renderer_draw(struct particle_renderer *system, struct camera *camera) {
	assert(system != NULL);
	assert(camera != NULL);
	if (system->particles_count <= 0)
		return;

	glBindVertexArray(system->vao);
	shader_use(&system->shader);
	shader_set_uniform_mat4(&system->shader, "u_projection", (float*)camera->projection);
	shader_set_uniform_mat4(&system->shader, "u_view"      , (float*)camera->view);

	glEnable(GL_DEPTH_TEST);
	glBindBuffer(GL_ARRAY_BUFFER, system->instance_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, system->particles_count * sizeof(struct particle_render_data), system->particle_render_data);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, system->particles_count);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	shader_use(NULL);
	glBindVertexArray(0);
}


//
// System specific
//

usize particle_renderer_spawn(struct particle_renderer *system, vec3 pos) {
	assert(system != NULL);

	// just to realloc to cpu & gpu buffer
	assert(system->particles_count < system->particles_capacity && "growing not supported yet");

	// init this particle
	usize particle_i = system->particles_count;
	struct particle_render_data *draw = &system->particle_render_data[particle_i];
	struct particle_data *particle = &system->particle_data[particle_i];
	glm_vec3_copy(pos, draw->pos);
	glm_vec2_one(draw->scale);
	glm_vec3_zero(particle->velocity);
	particle->lifetime = 1.0f;

	++system->particles_count;
	return particle_i;
}

void particle_renderer_update(struct particle_renderer *system, float dt) {
	assert(system != NULL);

	for (usize i = 0; i < system->particles_count; ++i) {
		struct particle_render_data *draw = &system->particle_render_data[i];
		struct particle_data *particle = &system->particle_data[i];

		// physics
		glm_vec3_add(draw->pos, particle->velocity, draw->pos);
		glm_vec3_scale(particle->velocity, 0.95f, particle->velocity);
		particle->velocity[1] -= 0.01f;

		particle->lifetime -= dt;
	}
}

