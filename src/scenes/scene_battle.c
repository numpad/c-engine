#include "scene_battle.h"

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
#include "game/cards.h"
#include "ecs/components.h"

static void reload_shader(struct engine_s *engine) {
	struct scene_battle_s *scene = (struct scene_battle_s *)engine->scene;
	scene->terrain->shader = shader_new("res/shader/isoterrain/vertex.glsl", "res/shader/isoterrain/fragment.glsl");
}

static void scene_battle_load(struct scene_battle_s *scene, struct engine_s *engine) {
	// TODO: remove, debug only
	stbds_arrput(engine->on_notify_callbacks, reload_shader);
	
	// ecs
	scene->world = ecs_init();
	ECS_COMPONENT(scene->world, c_card);
	ECS_COMPONENT(scene->world, c_handcard);

	// add some debug entities
	{
		ecs_entity_t e = ecs_new_id(scene->world);
		ecs_set(scene->world, e, c_card, { "Move Forward", 40 });
		ecs_set(scene->world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(scene->world);
		ecs_set(scene->world, e, c_card, { "Move Forward", 40 });
		ecs_set(scene->world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(scene->world);
		ecs_set(scene->world, e, c_card, { "Meal", 41 });
		ecs_set(scene->world, e, c_handcard, { 1 });
	}
	{
		ecs_entity_t e = ecs_new_id(scene->world);
		ecs_set(scene->world, e, c_card, { "Move Forward", 40 });
		ecs_set(scene->world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(scene->world);
		ecs_set(scene->world, e, c_card, { "Meal", 41 });
		ecs_set(scene->world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(scene->world);
		ecs_set(scene->world, e, c_card, { "Meal", 41 });
		ecs_set(scene->world, e, c_handcard, { 0 });
	}

	// component queries
	scene->q_handcards = ecs_query_init(scene->world, &(ecs_query_desc_t) {
		.filter.terms = {
			{ ecs_id(c_card) },
			{ ecs_id(c_handcard) },
		},
	});

	// isoterrain
	scene->terrain = malloc(sizeof(struct isoterrain_s));
	//isoterrain_init(scene->terrain, 10, 10, 3);
	isoterrain_init_from_file(scene->terrain, "res/data/levels/map2.json");

	// card renderer
	scene->cardrenderer = malloc(sizeof(struct cardrenderer_s));
	cardrenderer_init(scene->cardrenderer, "res/image/cards.png");

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

static void scene_battle_destroy(struct scene_battle_s *scene, struct engine_s *engine) {
	shader_delete(scene->bg_shader);
	vbuffer_destroy(scene->bg_vbuf);
	free(scene->bg_vbuf);
	isoterrain_destroy(scene->terrain);
	free(scene->terrain);
	cardrenderer_destroy(scene->cardrenderer);
	free(scene->cardrenderer);

	ecs_fini(scene->world);
}

static void scene_battle_update(struct scene_battle_s *scene, struct engine_s *engine, float dt) {
	int mx, my;
	Uint32 mstate = SDL_GetMouseState(&mx, &my);

	const float mxp = mx / (float)engine->window_width;

	ecs_iter_t it = ecs_query_iter(scene->world, scene->q_handcards);
	while (ecs_query_next(&it)) {
		c_card *cards = ecs_field(&it, c_card, 1);
		c_handcard *handcards = ecs_field(&it, c_handcard, 2);

		for (int i = 0; i < it.count; ++i) {
			handcards[i].selected = 0;
			if (mstate & SDL_BUTTON(1)) {
				if (i == (int)roundf(mxp * (it.count - 1.0f))) {
					handcards[i].selected = 1;
				}
			}
		}
	}

	glm_mat4_identity(engine->u_view);
	if (mstate & SDL_BUTTON(1) && my < engine->window_height * (float)0.6) {
		glm_scale(engine->u_view, (vec3){2.0f, 2.0f, 1.0f});
		glm_translate(engine->u_view, (vec3){
			(mx - engine->window_width / 2.0f) / -engine->window_width,
			(my - engine->window_height / 2.0f) / engine->window_height,
			0.0f
		});
	}
	glm_scale(engine->u_view, (vec3){0.35f, 0.35f, 1.0f});

}

static void scene_battle_draw(struct scene_battle_s *scene, struct engine_s *engine) {
	// draw bg
	glUseProgram(scene->bg_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, scene->bg_texture);
	vbuffer_draw(scene->bg_vbuf, 6);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	// draw terrain
	isoterrain_draw(scene->terrain, engine->u_projection, engine->u_view);

	// draw cards
	glUseProgram(scene->cardrenderer->shader);
	glUniformMatrix4fv(glGetUniformLocation(scene->cardrenderer->shader, "u_projection"), 1, GL_FALSE, engine->u_projection[0]);
	glUniformMatrix4fv(glGetUniformLocation(scene->cardrenderer->shader, "u_view"), 1, GL_FALSE, engine->u_view[0]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, scene->cardrenderer->tileset);

	ecs_iter_t it = ecs_query_iter(scene->world, scene->q_handcards);
	while (ecs_query_next(&it)) {
		c_card *cards = ecs_field(&it, c_card, 1);
		c_handcard *handcards = ecs_field(&it, c_handcard, 2);

		for (int i = 0; i < it.count; ++i) {
			const float angle = ((float)i / (it.count - 1)) * glm_rad(120.0f) - glm_rad(60.0f);
			const float x = sinf(angle) * 2.0f * glm_ease_circ_out(0.45f + fabsf((float)i / (it.count - 1.0f) - 0.5f) * 2.0f);
			const float y = cosf(angle) * 1.0f;
			float y_offset = 0.0f;
			float z_offset = 0.0f;
			if (handcards[i].selected) {
				y_offset += 0.8f;
				z_offset += 0.1f;
			}

			float card_scale = 0.375f;
			if (handcards[i].selected) {
				card_scale = 0.5f;
			}

			mat4 u_model;
			glm_mat4_identity(u_model);
			glm_scale(u_model, (vec3){ card_scale, card_scale, 1.0f });
			glm_translate(u_model, (vec3){ x, y * 0.6f + 0.4f + -(1.0f / card_scale) * engine->window_aspect + y_offset, 0.0f + z_offset });
			glm_rotate(u_model, angle * 0.2f, (vec3){0.0f, 0.0f, -1.0f});
			glUniformMatrix4fv(glGetUniformLocation(scene->cardrenderer->shader, "u_model"), 1, GL_FALSE, u_model[0]);
			const float step = 1.0f / 8.0f;
			const float tx = cards[i].image_id % 8;
			const float ty = floorf((float)cards[i].image_id / 8.0f);

			glUniform4fv(
				glGetUniformLocation(scene->cardrenderer->shader, "u_cardwindow_offset"),
				1, (vec4){tx * step, (ty + 1.0f) * step, step, -step});

			vbuffer_draw(&scene->cardrenderer->vbo, 6);
		}
	}

}

void scene_battle_init(struct scene_battle_s *scene_battle, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)scene_battle, engine);

	// init function pointers
	scene_battle->base.load = (scene_load_fn)scene_battle_load;
	scene_battle->base.destroy = (scene_destroy_fn)scene_battle_destroy;
	scene_battle->base.update = (scene_update_fn)scene_battle_update;
	scene_battle->base.draw = (scene_draw_fn)scene_battle_draw;

}

