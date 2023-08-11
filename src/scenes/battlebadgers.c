#include "battlebadgers.h"

#include <math.h>
#include <SDL_opengles2.h>
#include <SDL_net.h>
#include <nanovg.h>
#include <stb_ds.h>
#include <stb_image.h>
#include <cglm/cglm.h>
#include <flecs.h>
#include <cglm/cglm.h>
#include "engine.h"
#include "gl/texture.h"
#include "gl/shader.h"
#include "gl/vbuffer.h"
#include "game/isoterrain.h"

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

static void reload_shader(struct engine_s *engine) {
	struct battlebadgers_s *scene = engine->scene;
	scene->terrain->shader = shader_new("res/shader/isoterrain/vertex.glsl", "res/shader/isoterrain/fragment.glsl");
}

static void battlebadgers_load(struct battlebadgers_s *scene, struct engine_s *engine) {
	stbds_arrput(engine->on_notify_callbacks, reload_shader);
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

	// isoterrain
	scene->terrain = malloc(sizeof(struct isoterrain_s));
	isoterrain_init(scene->terrain, 10, 10);

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

	scene->bg_vbuf = malloc(sizeof(struct vbuffer_s));
	vbuffer_init(scene->bg_vbuf);
	vbuffer_set_data(scene->bg_vbuf, sizeof(vertices), vertices);
	vbuffer_set_attrib(scene->bg_vbuf, scene->bg_shader, "a_position", 2, GL_FLOAT, 0, 0);

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
	vbuffer_destroy(scene->bg_vbuf);
	free(scene->bg_vbuf);
	isoterrain_destroy(scene->terrain);
	free(scene->terrain);
	ecs_fini(scene->world);
}

static void battlebadgers_update(struct battlebadgers_s *scene, struct engine_s *engine, float dt) {
	int mx, my;
	Uint32 mstate = SDL_GetMouseState(&mx, &my);

	/*
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
	*/

	glm_mat4_identity(engine->u_view);
	if (mstate & SDL_BUTTON(1)) {
		glm_scale(engine->u_view, (vec3){2.0f, 2.0f, 1.0f});
	}

	glm_translate(engine->u_view, (vec3){-0.85f, -0.33f, 0.0f});
	if (mstate & SDL_BUTTON(1)) {
		glm_translate(engine->u_view, (vec3){
			(mx - engine->window_width / 2.0f) / -engine->window_width,
			(my - engine->window_height / 2.0f) / engine->window_height,
			0.0f
		});
	}
	glm_scale(engine->u_view, (vec3){0.35f, 0.35f, 1.0f});

}

static void battlebadgers_draw(struct battlebadgers_s *scene, struct engine_s *engine) {
	NVGcontext *vg = engine->vg;

	/*
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
	*/

	glUseProgram(scene->bg_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, scene->bg_texture);
	vbuffer_draw(scene->bg_vbuf, 6);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	isoterrain_draw(scene->terrain, engine->u_projection, engine->u_view);

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

