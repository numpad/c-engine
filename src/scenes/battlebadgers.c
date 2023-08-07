#include "battlebadgers.h"

#include <math.h>
#include <SDL_opengles2.h>
#include <SDL_net.h>
#include <nanovg.h>
#include <stb_ds.h>
#include <stb_image.h>
#include <cglm/cglm.h>
#include <flecs.h>
#include "engine.h"
#include "gl/texture.h"
#include "gl/shader.h"

typedef struct {
	float x, y;
} c_pos;

typedef struct {
	float x, y;
} c_vel;

typedef struct {
	float radius;
	float color[3];
} c_circle;

typedef struct {
	float angle;
} c_throwable;

static void battlebadgers_load(struct battlebadgers_s *scene, struct engine_s *engine) {
	// ecc
	scene->world = ecs_init();

	ECS_COMPONENT(scene->world, c_pos);
	ECS_COMPONENT(scene->world, c_vel);
	ECS_COMPONENT(scene->world, c_circle);

	for (int y = 0; y < 10; ++y) {
		for (int x = 0; x < 10; ++x) {
			ecs_entity_t e = ecs_new_id(scene->world);
			ecs_set(scene->world, e, c_pos, { 50.0f + 40.0f * x, 250.0f + 35.0f * y });
			ecs_set(scene->world, e, c_circle, { 10.0f, {0.2f, 0.3f, 0.8f }});
		}
	}

	ecs_entity_t e = ecs_new_id(scene->world);
	ecs_set(scene->world, e, c_pos, { engine->window_width * 0.5f, 40.0f });
	ecs_set(scene->world, e, c_vel, { -2.0f, 1.0f });
	ecs_set(scene->world, e, c_circle, { 10.0f, {0.8f, 0.3f, 0.3f }});

	scene->q_render = ecs_query_init(scene->world, &(ecs_query_desc_t) {
		.filter.terms = {
			{ ecs_id(c_pos) },
			{ ecs_id(c_circle) },
		},
	});
	scene->q_update_pos = ecs_query_init(scene->world, &(ecs_query_desc_t) {
		.filter.terms = {
			{ ecs_id(c_pos) },
			{ ecs_id(c_vel) },
		},
	});

	// background
	scene->bg_texture = texture_from_image("res/image/space_bg.png", NULL);

	// prepare background vertices
	scene->bg_shader = shader_new("res/shader/background/vertex.glsl", "res/shader/background/fragment.glsl");
	GLfloat vertices[] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f, 1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f,
	};

	glUseProgram(scene->bg_shader);

	glGenBuffers(1, &scene->bg_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, scene->bg_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	GLint a_position = glGetAttribLocation(scene->bg_shader, "a_position");
	glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(a_position);

	GLfloat mvp[] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	GLint u_mvp = glGetUniformLocation(scene->bg_shader, "u_mvp");
	glUniform4fv(u_mvp, 4, mvp);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);

}

static void battlebadgers_destroy(struct battlebadgers_s *scene, struct engine_s *engine) {
	shader_delete(scene->bg_shader);
	ecs_fini(scene->world);
}

static void battlebadgers_update(struct battlebadgers_s *scene, struct engine_s *engine, float dt) {
	const float GRAVITY = 0.2f;

	int mx, my;
	Uint32 mstate = SDL_GetMouseState(&mx, &my);

	ecs_iter_t it = ecs_query_iter(scene->world, scene->q_update_pos);
	while (ecs_query_next(&it)) {
		c_pos *pos = ecs_field(&it, c_pos, 1);
		c_vel *vel = ecs_field(&it, c_vel, 2);

		for (int i = 0; i < it.count; ++i) {
			vel[i].y += GRAVITY;
			pos[i].x += vel[i].x;
			pos[i].y += vel[i].y;
		}
	}
}

static void battlebadgers_draw(struct battlebadgers_s *scene, struct engine_s *engine) {
	NVGcontext *vg = engine->vg;

	ecs_iter_t it = ecs_query_iter(scene->world, scene->q_render);
	while (ecs_query_next(&it)) {
		c_pos *pos = ecs_field(&it, c_pos, 1);
		c_circle *circle = ecs_field(&it, c_circle, 2);

		for (int i = 0; i < it.count; ++i) {
			nvgBeginPath(vg);
			nvgCircle(vg, pos[i].x, pos[i].y, circle[i].radius);
			nvgFillPaint(vg, nvgRadialGradient(vg, pos[i].x - 5.0f, pos[i].y - 5.0f, 2.0f, 15.0f, nvgRGBf(1.0f, 1.0f, 1.0f), nvgRGBf(circle[i].color[0], circle[i].color[1], circle[i].color[2])));
			nvgFill(vg);

			nvgBeginPath(vg);
			nvgCircle(vg, pos[i].x, pos[i].y, circle[i].radius);
			nvgStrokeColor(vg, nvgRGBf(0.2f, 0.2f, 0.2f));
			nvgStrokeWidth(vg, 3.0f);
			nvgStroke(vg);
		}
	}

	glUseProgram(scene->bg_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, scene->bg_texture);
	glBindBuffer(GL_ARRAY_BUFFER, scene->bg_vbo);
	glEnableVertexAttribArray(glGetAttribLocation(scene->bg_shader, "a_position"));
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

}

void battlebadgers_init(struct battlebadgers_s *battlebadgers, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)battlebadgers, engine);

	// init function pointers
	battlebadgers->base.load = (scene_load_fn)battlebadgers_load;
	battlebadgers->base.destroy = (scene_destroy_fn)battlebadgers_destroy;
	battlebadgers->base.update = (scene_update_fn)battlebadgers_update;
	battlebadgers->base.draw = (scene_draw_fn)battlebadgers_draw;

}

