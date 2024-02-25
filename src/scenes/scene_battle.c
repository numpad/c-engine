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
#include "gl/graphics2d.h"
#include "game/isoterrain.h"
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


//
// vars
//

// game state
struct isoterrain_s *g_terrain;
static texture_t g_cards_texture;
static shader_t g_cards_shader;
static pipeline_t g_cards_pipeline;

ecs_world_t *g_world;
ecs_query_t *g_q_handcards;

// testing
static Mix_Chunk *sound;


//
// scene functions
//


static void load(struct scene_battle_s *scene, struct engine_s *engine) {
	engine_set_clear_color(0.34f, 0.72f, 0.98f);

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
	isoterrain_init_from_file(g_terrain, "res/data/levels/winter.json");

	// card renderer
	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	texture_init_from_image(&g_cards_texture, "res/image/cards.png", &settings);
	shader_init_from_dir(&g_cards_shader, "res/shader/sprite/");

	pipeline_init(&g_cards_pipeline, &g_cards_shader, 128);
	g_cards_pipeline.texture = &g_cards_texture;

	// background
	background_set_parallax("res/image/bg-glaciers/%d.png", 4);
	background_set_parallax_offset(-0.4f);

	sound = Mix_LoadWAV("res/sounds/test.wav");
}

static void destroy(struct scene_battle_s *scene, struct engine_s *engine) {
	background_destroy();
	isoterrain_destroy(g_terrain);
	free(g_terrain);
	texture_destroy(&g_cards_texture);
	shader_destroy(&g_cards_shader);
	pipeline_destroy(&g_cards_pipeline);
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
}

static void draw(struct scene_battle_s *scene, struct engine_s *engine) {
	background_draw(engine);

	// draw terrain
	const float t_padding = 40.0f;
	const float t_scale = ((engine->window_width - t_padding) / (float)g_terrain->projected_width);
	glm_mat4_identity(engine->u_view);
	glm_translate_x(engine->u_view, t_padding * 0.5f);
	glm_scale(engine->u_view, (float[]){t_scale, t_scale, t_scale});
	isoterrain_draw(g_terrain, engine);

	// draw cards
	glm_mat4_identity(engine->u_view);
	pipeline_reset(&g_cards_pipeline);

	ecs_iter_t it = ecs_query_iter(g_world, g_q_handcards);
	while (ecs_query_next(&it)) {
		c_card *cards = ecs_field(&it, c_card, 1);
		c_handcard *handcards = ecs_field(&it, c_handcard, 2);

		float card_z = 0.0f;
		for (int i = 0; i < it.count; ++i) {
			const float p = ((float)i / glm_max(it.count - 1, 1));
			const float angle = p * glm_rad(30.0f) - glm_rad(15.0f);
			const float x = (engine->window_width - 140.0f) * p + 70.0f;
			const float y = engine->window_height - 70.0f + 30.0f * fabsf(p - 0.5f) * 2.0f;
			float z_offset = 0.0f;
			
			card_z += 0.01f;

			float card_scale = 100.0f;
			if (handcards[i].selected) {
				z_offset = 0.8f;
				card_scale = 120.0f;
			}

			const float step = 1.0f / 8.0f;
			const float tx = cards[i].image_id % 8;
			const float ty = floorf((float)cards[i].image_id / 8.0f);
			//shader_set_uniform_vec4(g_cards_pipeline.shader, "u_cardwindow_offset", (vec4){tx * step, (ty + 1.0f) * step, step, -step});

			drawcmd_t cmd_card = DRAWCMD_INIT;
			cmd_card.size.x = 90;
			cmd_card.size.y = 128;
			drawcmd_t cmd_img = DRAWCMD_INIT;
			cmd_img.size.x = 90;
			cmd_img.size.y = 67;
			if (handcards[i].selected) {
				// card
				cmd_card.size.x *= 2.0f;
				cmd_card.size.y *= 2.0f;
				cmd_card.position.x = handcards[i].drag_x;
				cmd_card.position.y = y - cmd_card.size.y + handcards[i].drag_y * 0.1f;
				// img
				cmd_img.size.x *= 2.0f;
				cmd_img.size.y *= 2.0f;
			} else {
				cmd_card.position.x = x;
				cmd_card.position.y = y;
				cmd_card.angle = angle;
			}
			// card
			cmd_card.position.x -= cmd_card.size.x * 0.5f;
			cmd_card.position.y -= cmd_card.size.y * 0.5f;
			cmd_card.position.z = card_z + z_offset;
			// img
			glm_vec3_dup(cmd_card.position.raw, cmd_img.position.raw);
			cmd_img.angle = cmd_card.angle;
			cmd_img.position.x = cmd_card.position.x;
			cmd_img.position.y = cmd_card.position.y;
			cmd_img.position.z = cmd_card.position.z;
			cmd_img.origin.x = 0.5f;
			cmd_img.origin.y = 0.0f;
			cmd_img.origin.z = 0.0f;
			cmd_img.origin.w = cmd_card.size.y * 0.5f;
			drawcmd_set_texture_subrect(&cmd_img, g_cards_pipeline.texture, 90 * 2, 0, 90, 67);
			pipeline_emit(&g_cards_pipeline, &cmd_img);
			drawcmd_set_texture_subrect_tile(&cmd_card, g_cards_pipeline.texture, 90, 128, 0, 0);
			pipeline_emit(&g_cards_pipeline, &cmd_card);
		}
	}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	pipeline_draw(&g_cards_pipeline, engine);
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

