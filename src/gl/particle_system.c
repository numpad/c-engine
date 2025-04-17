#include "gl/particle_system.h"

#include <assert.h>
#include "gl/camera.h"
#include "gl/shader.h"
#include "gl/texture.h"
#include "util/util.h"

struct particle_vertex {
	float pos[2];
	float texcoord[2];
};

static struct particle_vertex quad_vertices[] = {
		(struct particle_vertex){
			.pos[0]=-0.5f, .pos[1]=-0.5f,
			.texcoord={ 0.0f, 1.0f },

		},
		(struct particle_vertex){
			.pos[0]= 0.5f, .pos[1]=-0.5f,
			.texcoord={ 1.0f, 1.0f },
		},
		(struct particle_vertex){
			.pos[0]=-0.5f, .pos[1]= 0.5f,
			.texcoord={ 0.0f, 0.0f },
		},
		(struct particle_vertex){
			.pos[0]= 0.5f, .pos[1]= 0.5f,
			.texcoord={ 1.0f, 0.0f },
		}
	};

static void delete_particle(struct particle_renderer *system, usize i);

//
// Renderer specific
//

void particle_renderer_init(struct particle_renderer *system) {
	assert(system != NULL);

	// init system
	system->particles_count      = 0;
	system->particles_capacity   = 2048; // TODO: reduce and implement growing buffer
	system->particle_render_data = malloc(system->particles_capacity * sizeof(struct particle_render_data));
	system->particle_data        = malloc(system->particles_capacity * sizeof(struct particle_data));
	glGenBuffers(1, &system->instance_vbo);

	// init renderer
	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	settings.flip_y = 0;
	settings.filter_min = GL_LINEAR;
	settings.filter_mag = GL_LINEAR;
	settings.wrap_s = GL_CLAMP_TO_EDGE;
	settings.wrap_t = GL_CLAMP_TO_EDGE;
	texture_init_from_image(&system->texture, "res/image/particles.png", &settings);
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
	GLuint a_position             = glGetAttribLocation(system->shader.program, "POSITION");
	GLuint a_texcoord             = glGetAttribLocation(system->shader.program, "TEXCOORD");
	GLuint a_instance_position    = glGetAttribLocation(system->shader.program, "INSTANCE_POSITION");
	GLuint a_instance_scale       = glGetAttribLocation(system->shader.program, "INSTANCE_SCALE");
	GLuint a_instance_color       = glGetAttribLocation(system->shader.program, "INSTANCE_COLOR");
	GLuint a_instance_tex_subrect = glGetAttribLocation(system->shader.program, "INSTANCE_TEXTURE_SUBRECT");
	glBindBuffer(GL_ARRAY_BUFFER, system->vbo);
	gl_check glEnableVertexAttribArray(a_position);
	gl_check glEnableVertexAttribArray(a_texcoord);
	assert(offsetof(struct particle_vertex, pos)      == 0);
	assert(offsetof(struct particle_vertex, texcoord) == 2 * sizeof(float));

	// base attribs
	// TODO: pack position & texcoord into a single vec4
	gl_check glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE, sizeof(struct particle_vertex), (void*)offsetof(struct particle_vertex, pos));
	gl_check glVertexAttribPointer(a_texcoord, 2, GL_FLOAT, GL_FALSE, sizeof(struct particle_vertex), (void*)offsetof(struct particle_vertex, texcoord));

	// instance attribs
	glBindBuffer(GL_ARRAY_BUFFER, system->instance_vbo);
	// attrib: instance position
	assert(offsetof(struct particle_render_data, pos) == 0 * sizeof(float));
	gl_check glEnableVertexAttribArray(a_instance_position);
	gl_check glVertexAttribPointer(a_instance_position,    3, GL_FLOAT, GL_FALSE, sizeof(struct particle_render_data), (void*)offsetof(struct particle_render_data, pos));
	glVertexAttribDivisor(a_instance_position, 1);
	// attrib: instance scale
	assert(offsetof(struct particle_render_data, scale) == 4 * sizeof(float));
	gl_check glEnableVertexAttribArray(a_instance_scale);
	gl_check glVertexAttribPointer(a_instance_scale,       2, GL_FLOAT, GL_FALSE, sizeof(struct particle_render_data), (void*)offsetof(struct particle_render_data, scale));
	glVertexAttribDivisor(a_instance_scale, 1);
	// attrib: instance color
	assert(offsetof(struct particle_render_data, color) == 8 * sizeof(float));
	gl_check glEnableVertexAttribArray(a_instance_color);
	gl_check glVertexAttribPointer(a_instance_color,       4, GL_FLOAT, GL_FALSE, sizeof(struct particle_render_data), (void*)offsetof(struct particle_render_data, color));
	glVertexAttribDivisor(a_instance_color, 1);
	// attrib: instance texture_subrect
	assert(offsetof(struct particle_render_data, texture_subrect) == 12 * sizeof(float));
	gl_check glEnableVertexAttribArray(a_instance_tex_subrect);
	gl_check glVertexAttribPointer(a_instance_tex_subrect, 4, GL_FLOAT, GL_FALSE, sizeof(struct particle_render_data), (void*)offsetof(struct particle_render_data, texture_subrect));
	glVertexAttribDivisor(a_instance_tex_subrect, 1);

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

	// bind resources
	glBindVertexArray(system->vao);
	shader_use(&system->shader);
	shader_set_uniform_mat4(&system->shader,    "u_projection", (float*)camera->projection);
	shader_set_uniform_mat4(&system->shader,    "u_view",       (float*)camera->view);
	shader_set_uniform_texture(&system->shader, "u_texture",    GL_TEXTURE0, &system->texture);
	// update particle data
	glBindBuffer(GL_ARRAY_BUFFER, system->instance_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, system->particles_count * sizeof(struct particle_render_data), system->particle_render_data);

	// configure GL
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, system->particles_count);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// restore state
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE);

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
	glm_vec3_copy(pos, draw->pos);
	glm_vec2_one(draw->scale);
	glm_vec4_one(draw->color);

	float tex_w = system->texture.width;
	float tex_h = system->texture.height;
	float x = 16 * 3;
	float y = 0;
	float w = 16;
	float h = 16;
	draw->texture_subrect[0] = x / (float)tex_w;
	draw->texture_subrect[1] = y / (float)tex_h;
	draw->texture_subrect[2] = w / (float)tex_w;
	draw->texture_subrect[3] = h / (float)tex_h;

	struct particle_data *particle = &system->particle_data[particle_i];
	glm_vec3_zero(particle->velocity);
	glm_vec3_zero(particle->acceleration);
	particle->lifetime = 1.0f;
	particle->mass     = 1.0f;
	glm_vec3_zero(particle->force_acc);
	glm_vec3_zero(particle->gravity);
	glm_vec3_zero(particle->drag);
	particle->flags = PARTICLE_FLAGS_NONE;

	++system->particles_count;
	return particle_i;
}

void particle_renderer_update(struct particle_renderer *system, float dt) {
	assert(system != NULL);

	for (usize i = 0; i < system->particles_count; ++i) {
		struct particle_render_data *draw = &system->particle_render_data[i];
		struct particle_data *particle = &system->particle_data[i];

		// Physics
		const float fadeout_extra_time = 0.667f;
		particle->lifetime -= dt;
		const int is_dead = (particle->lifetime < 0.0f);
		const int is_fading_out = (is_dead && (particle->lifetime > fadeout_extra_time));
		if (is_dead) {
			if (IS_FLAG_SET(particle->flags, PARTICLE_FLAGS_SHRINK_ON_DEATH)) {
				glm_vec2_scale(draw->scale, 0.6f, draw->scale);
			}
			if (IS_FLAG_SET(particle->flags, PARTICLE_FLAGS_FADE_TO_BLACK)) {
				glm_vec3_scale(draw->color, 0.8f, draw->color);
			}
		}

		// forceAccum += gravity * mass;
		vec3 additional_force;
		glm_vec3_scale(particle->gravity, particle->mass, additional_force);
		glm_vec3_add(particle->force_acc, additional_force, particle->force_acc);
		// vec3 dragForce = -drag * velocity;
		vec3 drag_force;
		glm_vec3_negate_to(particle->drag, drag_force);
		glm_vec3_mul(drag_force, particle->velocity, drag_force);
		// forceAccum += dragForce;
		glm_vec3_add(particle->force_acc, drag_force, particle->force_acc);
		// acceleration (+?)= forceAccum / mass;
		vec3 additional_acceleration;
		glm_vec3_divs(particle->force_acc, particle->mass, additional_acceleration);
		glm_vec3_add(particle->acceleration, additional_acceleration, particle->acceleration);
		// velocity += acceleration * dt;
		vec3 additional_velocity;
		glm_vec3_scale(particle->acceleration, dt, additional_velocity);
		glm_vec3_add(particle->velocity, additional_velocity, particle->velocity);
		// position += velocity * dt;
		vec3 additional_position;
		glm_vec3_scale(particle->velocity, dt, additional_position);
		glm_vec3_add(draw->pos, additional_position, draw->pos);
		// forceAccum = vec3(0.0f);
		glm_vec3_zero(particle->force_acc);
		// TODO: reset or not? both? 
		// glm_vec3_zero(particle->acceleration);

		// old
		//glm_vec3_add(draw->pos, particle->velocity, draw->pos);
		//glm_vec3_scale(particle->velocity, 0.95f, particle->velocity);
		//particle->velocity[1] -= 0.01f;

		particle->lifetime -= dt;

		if (particle->lifetime < 0.0f - fadeout_extra_time) {
			delete_particle(system, i);
			--i;
		}
	}
}

//
// Static
//

static void delete_particle(struct particle_renderer *system, usize i) {
	assert(system != NULL);
	assert(system->particles_count > 0);
	assert(i < system->particles_count);

	const usize last_i = (system->particles_count - 1);
	memcpy(&system->particle_data[i],        &system->particle_data[last_i],        sizeof(struct particle_data));
	memcpy(&system->particle_render_data[i], &system->particle_render_data[last_i], sizeof(struct particle_render_data));
	system->particles_count -= 1;
}

