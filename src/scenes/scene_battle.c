#include "scene_battle.h"

#include <math.h>
#include <SDL_opengles2.h>
#include <SDL_net.h>
#include <SDL_mixer.h>
#include <nanovg.h>
#include <stb_ds.h>
#include <stb_image.h>
#include <cglm/cglm.h>
#include <flecs.h>
#include <cglm/cglm.h>
#include <nuklear.h>
#include "engine.h"
#include "gl/texture.h"
#include "gl/shader.h"
#include "gl/vbuffer.h"
#include "game/isoterrain.h"
#include "game/cards.h"
#include "game/background.h"
#include "gui/console.h"
#include "ecs/components.h"
#include "scenes/menu.h"

//
// structs & enums
//

// 
// private functions
//

static void reload_shader(struct engine_s *engine);

//
// vars
//

// game state
struct isoterrain_s *g_terrain;
struct cardrenderer_s *g_cardrenderer;

ecs_world_t *g_world;
ecs_query_t *g_q_handcards;

// testing
static Mix_Chunk *sound;


//
// scene functions
//


static void load(struct scene_battle_s *scene, struct engine_s *engine) {
	// TODO: remove, debug only
	stbds_arrput(engine->on_notify_callbacks, reload_shader);
	
	// ecs
	g_world = ecs_init();
	ECS_COMPONENT(g_world, c_card);
	ECS_COMPONENT(g_world, c_handcard);
	ECS_COMPONENT(g_world, c_blockpos);

	// add some debug cards
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Move Forward", 40 });
		ecs_set(g_world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Move Forward", 40 });
		ecs_set(g_world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Meal", 41 });
		ecs_set(g_world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Move Forward", 40 });
		ecs_set(g_world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Meal", 41 });
		ecs_set(g_world, e, c_handcard, { 0 });
	}
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Meal", 41 });
		ecs_set(g_world, e, c_handcard, { 0 });
	}
	
	// add some debug entities
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_blockpos, { 8, 4, 2 });
	}

	// component queries
	g_q_handcards = ecs_query_init(g_world, &(ecs_query_desc_t) {
		.filter.terms = {
			{ ecs_id(c_card) },
			{ ecs_id(c_handcard) },
		},
	});

	// isoterrain
	g_terrain = malloc(sizeof(struct isoterrain_s));
	//isoterrain_init(g_terrain, 10, 10, 1);
	isoterrain_init_from_file(g_terrain, "res/data/levels/map2.json");

	// card renderer
	g_cardrenderer = malloc(sizeof(struct cardrenderer_s));
	cardrenderer_init(g_cardrenderer, "res/image/cards.png");

	// background
	background_set_image("res/image/space_bg.png");

	sound = Mix_LoadWAV("res/sounds/test.wav");
}

static void destroy(struct scene_battle_s *scene, struct engine_s *engine) {
	background_destroy();
	isoterrain_destroy(g_terrain);
	free(g_terrain);
	cardrenderer_destroy(g_cardrenderer);
	free(g_cardrenderer);

	ecs_fini(g_world);
}

static void update(struct scene_battle_s *scene, struct engine_s *engine, float dt) {
	int mx, my;
	Uint32 mstate = SDL_GetMouseState(&mx, &my);

	const struct input_drag_s *drag = &(engine->input_drag);

	if ((drag->state == INPUT_DRAG_BEGIN && drag->begin_y > engine->window_height * 0.7f) || drag->state == INPUT_DRAG_END) {
		ecs_iter_t it = ecs_query_iter(g_world, g_q_handcards);
		while (ecs_query_next(&it)) {
			c_card *cards = ecs_field(&it, c_card, 1);
			c_handcard *handcards = ecs_field(&it, c_handcard, 2);

			for (int i = 0; i < it.count; ++i) {
				if (drag->state == INPUT_DRAG_END && handcards[i].selected == 1) {
					// dropped card anywhere

					if (drag->y > engine->window_height * 0.5f) {
						console_add_message(engine->console, (struct console_msg_s) { "Card put pack into hand..." });
					} else {
						char msg[64]; // console_add_message makes a copy, so we can use a temporary buffer
						sprintf(msg, "Played '%s'", cards[i].name);
						console_add_message(engine->console, (struct console_msg_s) { msg });
						ecs_delete(g_world, it.entities[i]);
						Mix_PlayChannel(-1, sound, 0);
					}
				}

				handcards[i].selected = 0;
				
				if (drag->state == INPUT_DRAG_BEGIN) {
					const int id = ((drag->begin_x / (float)engine->window_width)) * it.count;
					if (i == id) {
						handcards[i].selected = 1;
					}
				}
			}
		}
	}
	if (drag->state == INPUT_DRAG_BEGIN || drag->state == INPUT_DRAG_IN_PROGRESS) {
		ecs_iter_t it = ecs_query_iter(g_world, g_q_handcards);
		while (ecs_query_next(&it)) {
			c_card *cards = ecs_field(&it, c_card, 1);
			c_handcard *handcards = ecs_field(&it, c_handcard, 2);

			for (int i = 0; i < it.count; ++i) {
				if (handcards[i].selected) {
					handcards[i].drag_x = drag->x;
					handcards[i].drag_y = drag->y;
				}
			}
		}
	}

	// map transform
	const float mw = g_terrain->width * 16.0f;
	glm_mat4_identity(engine->u_view);
	glm_translate(engine->u_view, (vec3){ 0.0f, 0.0f, 0.0f });
	glm_scale(engine->u_view, (vec3){ engine->window_width / mw, -(engine->window_width / mw), 1.0f});
}

static void draw(struct scene_battle_s *scene, struct engine_s *engine) {
	background_draw();

	// draw terrain
	isoterrain_draw(g_terrain, engine->u_projection, engine->u_view);

	// draw cards
	glUseProgram(g_cardrenderer->shader.program);
	glUniformMatrix4fv(glGetUniformLocation(g_cardrenderer->shader.program, "u_projection"), 1, GL_FALSE, engine->u_projection[0]);
	glUniformMatrix4fv(glGetUniformLocation(g_cardrenderer->shader.program, "u_view"), 1, GL_FALSE, engine->u_view[0]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_cardrenderer->texture_atlas.texture);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	// TODO: z-sorting
	ecs_iter_t it = ecs_query_iter(g_world, g_q_handcards);
	while (ecs_query_next(&it)) {
		c_card *cards = ecs_field(&it, c_card, 1);
		c_handcard *handcards = ecs_field(&it, c_handcard, 2);

		float card_z = 0.0f;
		for (int i = 0; i < it.count; ++i) {
			const float p = ((float)i / glm_max(it.count - 1, 1));
			const float angle = p * glm_rad(120.0f) - glm_rad(60.0f);
			const float x = (engine->window_width - 140.0f) * p + 70.0f;
			const float y = engine->window_height - 70.0f + 20.0f * fabsf(p - 0.5f) * 2.0f;
			float z_offset = 0.0f;
			
			card_z += 0.01f;

			float card_scale = 100.0f;
			if (handcards[i].selected) {
				z_offset = 0.8f;
				card_scale = 120.0f;
			}

			mat4 u_model;
			glm_mat4_identity(u_model);
			if (handcards[i].selected) {
				glm_translate(u_model, (vec3){ handcards[i].drag_x, handcards[i].drag_y, card_z + z_offset});
				glm_scale(u_model, (vec3){ card_scale, -card_scale, 1.0f });
			} else {
				glm_translate(u_model, (vec3){ x, y, card_z});
				glm_rotate(u_model, angle * 0.2f, (vec3){0.0f, 0.0f, 1.0f});
				glm_scale(u_model, (vec3){ card_scale, -card_scale, 1.0f });
			}
			glUniformMatrix4fv(glGetUniformLocation(g_cardrenderer->shader.program, "u_model"), 1, GL_FALSE, u_model[0]);
			const float step = 1.0f / 8.0f;
			const float tx = cards[i].image_id % 8;
			const float ty = floorf((float)cards[i].image_id / 8.0f);

			glUniform4fv(
				glGetUniformLocation(g_cardrenderer->shader.program, "u_cardwindow_offset"),
				1, (vec4){tx * step, (ty + 1.0f) * step, step, -step});

			vbuffer_draw(&g_cardrenderer->vbo, 6);
		}
	}
	glDisable(GL_DEPTH_TEST);
}

void scene_battle_init(struct scene_battle_s *scene_battle, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)scene_battle, engine);

	// init function pointers
	scene_battle->base.load    = (scene_load_fn)load;
	scene_battle->base.destroy = (scene_destroy_fn)destroy;
	scene_battle->base.update  = (scene_update_fn)update;
	scene_battle->base.draw    = (scene_draw_fn)draw;

}

//
// private implementations
//

static void reload_shader(struct engine_s *engine) {
	struct scene_battle_s *scene = (struct scene_battle_s *)engine->scene;
	shader_init(&g_terrain->shader, "res/shader/isoterrain/vertex.glsl", "res/shader/isoterrain/fragment.glsl");
}

