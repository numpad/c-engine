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
#include "gl/canvas.h"
#include "game/isoterrain.h"
#include "game/background.h"
#include "gui/console.h"
#include "scenes/menu.h"
#include "util/util.h"

//
// structs & enums
//

// 
// private functions
//

static void recalculate_handcards(void);

//
// ecs
//

// components

// pos
typedef vec2s c_position;

// general information about a card
typedef struct {
	char *name;
	int image_id;
} c_card ;

// a cards state when held in hand
typedef struct {
	vec2s hand_target_pos;
	float hand_space;
	int is_selected;
} c_handcard;

// position on the isoterrain grid
typedef struct {
	int x, y, z;
} c_blockpos;

typedef struct {
	int id;
	int tile_width;
	int tile_height;
	int tileset_width;
	int tileset_height;
} c_tileset_tile;

ECS_COMPONENT_DECLARE(c_position);
ECS_COMPONENT_DECLARE(c_card);
ECS_COMPONENT_DECLARE(c_handcard);
ECS_COMPONENT_DECLARE(c_blockpos);
ECS_COMPONENT_DECLARE(c_tileset_tile);

// queries
static ecs_query_t *g_ordered_handcards;
static int g_handcards_updated;

// systems
static void system_draw_cards(ecs_iter_t *);
static void system_draw_entities(ecs_iter_t *);
static void system_move_cards(ecs_iter_t *);
static void observer_on_update_handcards(ecs_iter_t *);

ECS_SYSTEM_DECLARE(system_draw_cards);
ECS_SYSTEM_DECLARE(system_draw_entities);
ECS_SYSTEM_DECLARE(system_move_cards);

//
// vars
//
static engine_t *g_engine;

// game state
struct isoterrain_s *g_terrain;
static texture_t g_cards_texture;
static texture_t g_entities_texture;
static shader_t g_sprite_shader;
static pipeline_t g_cards_pipeline;
static pipeline_t g_entities_pipeline;

static ecs_world_t *g_world;
static ecs_entity_t g_selected_card;

// testing
static Mix_Chunk *g_place_card_sfx;
static Mix_Chunk *g_pick_card_sfx;
static Mix_Chunk *g_slide_card_sfx;


//
// scene functions
//

static void load(struct scene_battle_s *scene, struct engine_s *engine) {
	g_engine = engine;
	g_handcards_updated = 0;
	g_selected_card = 0;

	// ecs
	g_world = ecs_init();
	ECS_COMPONENT_DEFINE(g_world, c_position);
	ECS_COMPONENT_DEFINE(g_world, c_card);
	ECS_COMPONENT_DEFINE(g_world, c_handcard);
	ECS_COMPONENT_DEFINE(g_world, c_blockpos);
	ECS_COMPONENT_DEFINE(g_world, c_tileset_tile);
	ECS_SYSTEM_DEFINE(g_world, system_draw_cards, 0, c_card, c_handcard, c_position);
	ECS_SYSTEM_DEFINE(g_world, system_draw_entities, 0, c_blockpos, c_tileset_tile);
	ECS_SYSTEM_DEFINE(g_world, system_move_cards, 0, c_position, c_handcard);

	g_ordered_handcards = ecs_query(g_world, {
		.filter.terms = { {ecs_id(c_card)}, {ecs_id(c_handcard)} },
		//.order_by_component = ecs_id(c_handcard),
		//.order_by = order_handcards,
		});
	
	ecs_observer(g_world, {
		.filter.terms = { {ecs_id(c_card)}, {ecs_id(c_handcard)}, {ecs_id(c_position)}, },
		.events = { EcsOnAdd, EcsOnRemove, EcsOnSet },
		.callback = observer_on_update_handcards,
		});

	// add some debug cards
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Attack",     .image_id=0 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Attack",     .image_id=0 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Fire Spell", .image_id=4 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Defend",     .image_id=2 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Meal",       .image_id=1 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Corruption", .image_id=5 });
	}

	// add some debug entities
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_blockpos, { 4, 4, 1 });
		ecs_set(g_world, e, c_tileset_tile, { .id=0, .tile_width=16, .tile_height=0, .tileset_width=256, .tileset_height=256 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_blockpos, { 1, 8, 2 });
		ecs_set(g_world, e, c_tileset_tile, { .id=0, .tile_width=16, .tile_height=0, .tileset_width=256, .tileset_height=256 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_blockpos, { 7, 6, 1 });
		ecs_set(g_world, e, c_tileset_tile, { .id=0, .tile_width=16, .tile_height=0, .tileset_width=256, .tileset_height=256 });
	}

	// isoterrain
	g_terrain = malloc(sizeof(struct isoterrain_s));
	isoterrain_init_from_file(g_terrain, "res/data/levels/winter.json");

	// card renderer
	{
		struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
		settings.filter_min = GL_LINEAR;
		settings.filter_mag = GL_LINEAR;
		texture_init_from_image(&g_cards_texture, "res/image/cards.png", &settings);
		shader_init_from_dir(&g_sprite_shader, "res/shader/sprite/");

		pipeline_init(&g_cards_pipeline, &g_sprite_shader, 128);
	}

	// entity renderer
	{
		struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
		settings.filter_min = GL_LINEAR;
		settings.filter_mag = GL_NEAREST;
		texture_init_from_image(&g_entities_texture, "res/sprites/entities-outline.png", &settings);

		pipeline_init(&g_entities_pipeline, &g_sprite_shader, 128);
	}

	// background
	background_set_parallax("res/image/bg-glaciers/%d.png", 4);
	background_set_parallax_offset(-0.4f);

	// audio
	g_place_card_sfx = Mix_LoadWAV("res/sounds/place_card.ogg");
	g_pick_card_sfx = Mix_LoadWAV("res/sounds/cardSlide5.ogg");
	g_slide_card_sfx = Mix_LoadWAV("res/sounds/cardSlide7.ogg");
}

static void destroy(struct scene_battle_s *scene, struct engine_s *engine) {
	Mix_FreeChunk(g_place_card_sfx);
	Mix_FreeChunk(g_pick_card_sfx);
	background_destroy();
	isoterrain_destroy(g_terrain);
	free(g_terrain);
	texture_destroy(&g_cards_texture);
	texture_destroy(&g_entities_texture);
	shader_destroy(&g_sprite_shader);
	pipeline_destroy(&g_cards_pipeline);
	pipeline_destroy(&g_entities_pipeline);
	ecs_query_fini(g_ordered_handcards);
	ecs_fini(g_world);
}

static void update(struct scene_battle_s *scene, struct engine_s *engine, float dt) {
	const struct input_drag_s *drag = &(engine->input_drag);
	// find closest handcard
	if (drag->state == INPUT_DRAG_BEGIN) {
		vec2s cursor_pos = { .x = drag->begin_x, .y = drag->begin_y };
		ecs_entity_t e = 0;
		float closest = glm_pow2(110.0f); // max distance 110px

		ecs_iter_t it = ecs_query_iter(g_world, g_ordered_handcards);
		while (ecs_query_next(&it)) {
			//c_card *cards = ecs_field(&it, c_card, 1);
			c_handcard *handcards = ecs_field(&it, c_handcard, 2);

			for (int i = 0; i < it.count; ++i) {
				float d2 = glm_vec2_distance2(handcards[i].hand_target_pos.raw, cursor_pos.raw);
				if (d2 < closest) {
					closest = d2;
					e = it.entities[i];
				}
			}
		}

		g_selected_card = e;
		if (g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card)) {
			c_handcard *hc = ecs_get_mut(g_world, g_selected_card, c_handcard);
			hc->hand_space = 0.4f;
			hc->is_selected = 1;
			ecs_modified(g_world, g_selected_card, c_handcard);

			Mix_PlayChannel(-1, g_pick_card_sfx, 0);
		}
	}
	if (drag->state == INPUT_DRAG_END) {
		if (g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card)) {
			c_handcard *hc = ecs_get_mut(g_world, g_selected_card, c_handcard);
			hc->hand_space = 1.0f;
			hc->is_selected = 0;
			ecs_modified(g_world, g_selected_card, c_handcard);
			g_selected_card = 0;

			Mix_PlayChannel(-1, g_slide_card_sfx, 0);
		}
	}

	if (drag->state == INPUT_DRAG_IN_PROGRESS && g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card)) {
		c_position *pos = ecs_get_mut(g_world, g_selected_card, c_position);
		pos->x = drag->x;
		pos->y = drag->y;
	}

	// add cards
	static float card_add_accum = 0.0f;
	card_add_accum += dt;
	if (card_add_accum >= 0.3f) {
		card_add_accum -= 0.3f;
		
		ecs_filter_t *filter = ecs_filter_init(g_world, &(ecs_filter_desc_t){
			.terms = {
				{ .id = ecs_id(c_card) },
				{ .id = ecs_id(c_position), .oper = EcsNot },
				{ .id = ecs_id(c_handcard), .oper = EcsNot },
			},
		});

		ecs_iter_t it = ecs_filter_iter(g_world, filter);
		while (ecs_filter_next(&it)) {
			for (int i = 0; i < it.count; ++i) {
				ecs_entity_t e = it.entities[i];
				ecs_set(g_world, e, c_handcard, { .hand_space = 1.0f, .hand_target_pos = {0}, .is_selected = 0, });
				ecs_set(g_world, e, c_position, { .x = engine->window_width, .y = engine->window_height * 0.9f });
				Mix_PlayChannel(-1, g_place_card_sfx, 0);
				goto end;
			}
		}
end:
		while (ecs_filter_next(&it));
		ecs_filter_fini(filter);
	}

	static int last_width = -1;
	static int last_height = -1;
	if (g_handcards_updated || last_width != engine->window_width || last_height != engine->window_height) {
		g_handcards_updated = 0;
		last_width = engine->window_width;
		last_height = engine->window_height;

		recalculate_handcards();
	}
}


static void draw(struct scene_battle_s *scene, struct engine_s *engine) {
	engine_set_clear_color(0.34f, 0.72f, 0.98f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// draw background
	background_draw(engine);

	// draw terrain
	const float t_padding = 40.0f;
	const float t_scale = ((engine->window_width - t_padding) / (float)g_terrain->projected_width);
	const float t_y = engine->window_height * 0.5f - g_terrain->projected_height * t_scale * 0.5f;
	glm_mat4_identity(engine->u_view);
	glm_translate_x(engine->u_view, t_padding * 0.5f);
	glm_translate_y(engine->u_view, t_y);
	glm_scale(engine->u_view, (float[]){t_scale, t_scale, t_scale});
	isoterrain_draw(g_terrain, engine);

	// drawing systems
	pipeline_reset(&g_entities_pipeline);
	g_entities_pipeline.texture = &g_entities_texture;

	pipeline_reset(&g_cards_pipeline);
	g_cards_pipeline.texture = &g_cards_texture;

	ecs_run(g_world, ecs_id(system_move_cards), engine->dt, NULL);
	ecs_run(g_world, ecs_id(system_draw_cards), engine->dt, NULL);
	ecs_run(g_world, ecs_id(system_draw_entities), engine->dt, NULL);

	pipeline_draw(&g_entities_pipeline, engine);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	pipeline_draw_ortho(&g_cards_pipeline, g_engine->window_width, g_engine->window_height);
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

static void recalculate_handcards(void) {
	// count number of cards
	int cards_count = 0;
	{
		ecs_iter_t it = ecs_query_iter(g_world, g_ordered_handcards);
		while (ecs_query_next(&it)) {
			cards_count += it.count;
		}
	}
	const float hand_max_width = fminf(g_engine->window_width - 60.0f, 500.0f);
	const float card_width = fminf(hand_max_width / fmaxf(cards_count, 1.0f), 75.0f);

	// calculate width of hand cards
	float stacked_cards_width = card_width;
	{
		float previous_card_width = card_width;
		ecs_iter_t it = ecs_query_iter(g_world, g_ordered_handcards);
		while (ecs_query_next(&it)) {
			c_handcard *handcards = ecs_field(&it, c_handcard, 2);
			for (int i = 0; i < it.count; ++i) {
				stacked_cards_width += (previous_card_width * 0.5f) + (card_width * handcards[i].hand_space * 0.5f);
				previous_card_width = card_width * handcards[i].hand_space;
			}
		}
	}

	// set handcard positions
	const float hand_center = g_engine->window_width * 0.5f;
	float previous_card_width = card_width;
	float current_x = 0.0f;

	int card_i = 0;
	ecs_iter_t it = ecs_query_iter(g_world, g_ordered_handcards);
	while (ecs_query_next(&it)) {
		//c_card *cards = ecs_field(&it, c_card, 1);
		c_handcard *handcards = ecs_field(&it, c_handcard, 2);

		for (int i = 0; i < it.count; ++i) {
			current_x += (previous_card_width * 0.5f) + (card_width * handcards[i].hand_space * 0.5f);

			const float p = 1.0f - fabsf((card_i / glm_max(cards_count - 1.0f, 1.0f)) * 2.0f - 1.0f);
			handcards[i].hand_target_pos.x = hand_center - (stacked_cards_width * 0.5f) + current_x;
			handcards[i].hand_target_pos.y = g_engine->window_height - 50.0f - p * 20.0f;

			++card_i;
			previous_card_width = card_width * handcards[i].hand_space;
		}
	}
}

//
// systems implementation
//

static void system_draw_cards(ecs_iter_t *it) {
	c_card *cards = ecs_field(it, c_card, 1);
	c_handcard *handcards = ecs_field(it, c_handcard, 2);
	c_position *positions = ecs_field(it, c_position, 3);

	// TODO: this only works for a single archetype
	const int cards_count = it->count;

	float card_z = 0.0f;
	for (int i = 0; i < cards_count; ++i) {
		vec2s *card_pos = &positions[i];
		const float p = ((float)i / glm_max(cards_count - 1, 1));
		float angle = p * glm_rad(30.0f) - glm_rad(15.0f);
		if (handcards[i].is_selected) {
			angle = 0.0f;
		}
		float z_offset = handcards[i].is_selected * 0.1f;
		
		card_z += 0.01f;

		drawcmd_t cmd_card = DRAWCMD_INIT;
		cmd_card.size.x = 90;
		cmd_card.size.y = 128;
		cmd_card.position.x = card_pos->x;
		cmd_card.position.y = card_pos->y;
		cmd_card.position.z = card_z + z_offset;
		cmd_card.angle = angle;
		cmd_card.position.x -= cmd_card.size.x * 0.5f;
		cmd_card.position.y -= cmd_card.size.y * 0.5f;
		drawcmd_t cmd_img = DRAWCMD_INIT;
		cmd_img.size.x = 90;
		cmd_img.size.y = 64;
		glm_vec3_dup(cmd_card.position.raw, cmd_img.position.raw);
		cmd_img.angle = cmd_card.angle;
		cmd_img.origin.x = 0.5f;
		cmd_img.origin.y = 0.0f;
		cmd_img.origin.z = 0.0f;
		cmd_img.origin.w = cmd_card.size.y * 0.5f;
		// img
		drawcmd_set_texture_subrect(&cmd_img, g_cards_pipeline.texture, 90 * (1 + cards[i].image_id % 4), 64 * floorf(cards[i].image_id / 4.0f), 90, 64);
		pipeline_emit(&g_cards_pipeline, &cmd_img);
		// card
		drawcmd_set_texture_subrect_tile(&cmd_card, g_cards_pipeline.texture, 90, 128, 0, 0);
		pipeline_emit(&g_cards_pipeline, &cmd_card);
	}
}

static void system_draw_entities(ecs_iter_t *it) {
	c_blockpos *positions = ecs_field(it, c_blockpos, 1);
	c_tileset_tile *tiles = ecs_field(it, c_tileset_tile, 2);
	
	for (int i = 0; i < it->count; ++i) {
		vec2 screen_pos;
		c_blockpos blockpos = positions[i];
		isoterrain_pos_block_to_screen(g_terrain, blockpos.x, blockpos.y, blockpos.z, screen_pos);

		drawcmd_t cmd = DRAWCMD_INIT;
		cmd.size.x = 16;
		cmd.size.y = 16;
		glm_vec2(screen_pos, cmd.position.raw);
		cmd.position.y = g_terrain->projected_height - cmd.position.y;
		drawcmd_set_texture_subrect_tile(&cmd, g_entities_pipeline.texture, 16, 17, 0, 0);

		pipeline_emit(&g_entities_pipeline, &cmd);
		
	}
}

static void system_move_cards(ecs_iter_t *it) {
	c_position *positions = ecs_field(it, c_position, 1);
	c_handcard *handcards = ecs_field(it, c_handcard, 2);

	for (int i = 0; i < it->count; ++i) {
		if (it->entities[i] == g_selected_card) {
			continue;
		}

		vec2s *p = &positions[i];
		vec2s *target = &handcards[i].hand_target_pos;

		glm_vec2_lerp(p->raw, target->raw, it->delta_time * 9.0f, p->raw);
	}
}

static void observer_on_update_handcards(ecs_iter_t *it) {
	c_card *changed_cards = ecs_field(it, c_card, 1);
	c_handcard *changed_handcards = ecs_field(it, c_handcard, 2);

	g_handcards_updated = 1;
}

