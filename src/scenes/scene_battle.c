#include "scene_battle.h"

#include <math.h>
#include <time.h>
#include <SDL_opengles2.h>
#include <SDL_net.h>
#include <SDL_mixer.h>
#include <nanovg.h>
#include <stb_ds.h>
#include <stb_image.h>
#include <stb_perlin.h>
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
#include "scenes/menu.h"
#include "util/util.h"

//
// structs & enums
//

enum block_type {
	BLOCK_AIR = -1,
	BLOCK_DIRT = 0,
	BLOCK_GRASS = 1,
	BLOCK_STONE = 2,
	BLOCK_ICE = 33,
};

enum event_type {
	EVENT_PLAY_CARD,
	EVENT_TYPE_MAX,
};

typedef struct event_info {
	ecs_entity_t entity;
} event_info_t;

// 
// private functions
//

static void recalculate_handcards(void);
static ecs_entity_t find_closest_handcard(float x, float y, float max_distance);
static int order_handcards(ecs_entity_t e1, const void *data1, ecs_entity_t e2, const void *data2);
static void on_game_event(enum event_type, event_info_t *);
static void on_game_event_play_card(event_info_t *info);

//
// ecs
//

// components

// pos
typedef vec2s c_position;

// general information about a card
typedef struct {
	char *name;
	int   image_id;
	int   icon_ids[8];
	int   icon_ids_count;
} c_card;

// a cards state when held in hand
typedef struct {
	vec2s hand_target_pos;
	float hand_space;
	int   is_selected;
	int   can_be_placed;
	float added_at_time;
} c_handcard;

// position on the isoterrain grid
typedef struct {
	int x, y, z;
} c_blockpos;

typedef struct {
	int x, y, z;
} c_move_direction;

typedef struct {
	int tile_width;
	int tile_height;
	int tile_x;
	int tile_y;
	int flip_x;
	int backside;
} c_tileset_tile;

ECS_COMPONENT_DECLARE(c_position);
ECS_COMPONENT_DECLARE(c_card);
ECS_COMPONENT_DECLARE(c_handcard);
ECS_COMPONENT_DECLARE(c_blockpos);
ECS_COMPONENT_DECLARE(c_move_direction);
ECS_COMPONENT_DECLARE(c_tileset_tile);

// queries
static ecs_query_t *g_ordered_handcards;
static int g_handcards_updated;

// systems
static void system_turn_move_entities   (ecs_iter_t *);
static void system_move_cards           (ecs_iter_t *);
static void system_draw_cards           (ecs_iter_t *);
static void system_draw_entities        (ecs_iter_t *);
static void observer_on_update_handcards(ecs_iter_t *);

ECS_SYSTEM_DECLARE(system_turn_move_entities);
ECS_SYSTEM_DECLARE(system_move_cards);
ECS_SYSTEM_DECLARE(system_draw_cards);
ECS_SYSTEM_DECLARE(system_draw_entities);

//
// vars
//
static engine_t     *g_engine;

// game state
struct isoterrain_s g_terrain;
static texture_t    g_cards_texture;
static texture_t    g_entities_texture;
static texture_t    g_ui_texture;
static shader_t     g_sprite_shader;
static pipeline_t   g_cards_pipeline;
static pipeline_t   g_entities_pipeline;
static pipeline_t   g_ui_pipeline;
static ecs_world_t  *g_world;
static ecs_entity_t g_selected_card;
static int          g_next_turn;

// testing
static Mix_Chunk    *g_place_card_sfx;
static Mix_Chunk    *g_pick_card_sfx;
static Mix_Chunk    *g_slide_card_sfx;


//
// scene functions
//

static void load(struct scene_battle_s *scene, struct engine_s *engine) {
	g_engine = engine;
	g_handcards_updated = 0;
	g_selected_card = 0;
	g_next_turn = 0;

	// ecs
	g_world = ecs_init();
	ECS_COMPONENT_DEFINE(g_world, c_position);
	ECS_COMPONENT_DEFINE(g_world, c_card);
	ECS_COMPONENT_DEFINE(g_world, c_handcard);
	ECS_COMPONENT_DEFINE(g_world, c_blockpos);
	ECS_COMPONENT_DEFINE(g_world, c_move_direction);
	ECS_COMPONENT_DEFINE(g_world, c_tileset_tile);
	ECS_SYSTEM_DEFINE(g_world, system_turn_move_entities, 0, c_blockpos, c_move_direction);
	ECS_SYSTEM_DEFINE(g_world, system_move_cards, 0, c_position, c_handcard);
	ECS_SYSTEM_DEFINE(g_world, system_draw_cards, 0, c_card, ?c_handcard, c_position); _syntax_fix_label:
	ECS_SYSTEM_DEFINE(g_world, system_draw_entities, 0, c_blockpos, c_tileset_tile);

	g_ordered_handcards = ecs_query(g_world, {
		.filter.terms = { {ecs_id(c_card)}, {ecs_id(c_handcard)} },
		.order_by_component = ecs_id(c_handcard),
		.order_by = order_handcards,
		});
	
	ecs_observer(g_world, {
		.filter.terms = { {ecs_id(c_card)}, {ecs_id(c_handcard)}, {ecs_id(c_position)}, },
		.events = { EcsOnAdd, EcsOnRemove, EcsOnSet },
		.callback = observer_on_update_handcards,
		});

	// add some debug cards
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Attack",     .image_id=0, .icon_ids_count=1, .icon_ids={1} });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Attack",     .image_id=0, .icon_ids_count=1, .icon_ids={1} });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Fire Spell", .image_id=4, .icon_ids_count=1, .icon_ids={3} });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Defend",     .image_id=2, .icon_ids_count=1, .icon_ids={2} });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Meal",       .image_id=1, .icon_ids_count=1, .icon_ids={5} });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { .name="Corruption", .image_id=5, .icon_ids_count=3, .icon_ids={3, 3, 4} });
	}

	// add some debug entities
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set_name(g_world, e, "player");
		ecs_set(g_world, e, c_blockpos, { 4, 4, 19 });
		ecs_set(g_world, e, c_tileset_tile, { .tile_width=16, .tile_height=17, .tile_x=0, .tile_y=0 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_blockpos, { 1, 8, 20 });
		ecs_set(g_world, e, c_tileset_tile, { .tile_width=16, .tile_height=17, .tile_x=8, .tile_y=6 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_blockpos, { 7, 6, 19 });
		ecs_set(g_world, e, c_tileset_tile, { .tile_width=16, .tile_height=17, .tile_x=4, .tile_y=0, .flip_x=1 });
	}

	// isoterrain
	//isoterrain_init_from_file(&g_terrain, "res/data/levels/winter.json");
	isoterrain_init(&g_terrain, 15, 15, 20);
	for (int z = 0; z < g_terrain.layers; ++z) {
		for (int y = 0; y < g_terrain.height; ++y) {
			for (int x = 0; x < g_terrain.width; ++x) {
				isoterrain_set_block(&g_terrain, x, y, z, -1);

				if (x == g_terrain.width - 1 || y == 0) {
					enum block_type b = BLOCK_AIR;
					if (z < 12) {
						b = BLOCK_STONE;
					}
					else if (z <= 17) {
						float noise = stb_perlin_noise3_seed(x / 10.0f, y / 10.0f, z / 10.0f, 0, 0, 0, time(NULL))
									+ stb_perlin_noise3_seed(x /  3.0f, y /  3.0f, z /  3.0f, 0, 0, 0, time(NULL));
						if (noise > 0.0f) {
							b = BLOCK_STONE;
						} else {
							b = BLOCK_DIRT;
						}
					}
					isoterrain_set_block(&g_terrain, x, y, z, b);
				}

				if (z == 18) {
					isoterrain_set_block(&g_terrain, x, y, z, BLOCK_GRASS);
				}
				else if (z == 19) {
					float noise = stb_perlin_noise3_seed(x / 10.0f, y / 10.0f, z / 10.0f, 0, 0, 0, time(NULL))
								+ stb_perlin_noise3_seed(x /  3.0f, y /  3.0f, z /  3.0f, 0, 0, 0, time(NULL));
					if (noise > 0.0f) {
						isoterrain_set_block(&g_terrain, x, y, z, BLOCK_DIRT);
					}
				}
			}
		}
	}


	// card renderer
	{
		struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
		settings.filter_min = GL_LINEAR;
		settings.filter_mag = GL_LINEAR;
		texture_init_from_image(&g_cards_texture, "res/image/cards.png", &settings);
		shader_init_from_dir(&g_sprite_shader, "res/shader/sprite/");

		pipeline_init(&g_cards_pipeline, &g_sprite_shader, 128);
		g_cards_pipeline.z_sorting_enabled = 1;
	}

	// entity renderer
	{
		struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
		settings.filter_min = GL_LINEAR;
		settings.filter_mag = GL_NEAREST;
		texture_init_from_image(&g_entities_texture, "res/sprites/entities-outline.png", &settings);

		pipeline_init(&g_entities_pipeline, &g_sprite_shader, 128);
	}

	// ui
	{
		struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
		texture_init_from_image(&g_ui_texture, "res/image/ui.png", &settings);
		pipeline_init(&g_ui_pipeline, &g_sprite_shader, 128);
		g_ui_pipeline.texture = &g_ui_texture;
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
	Mix_FreeChunk(g_slide_card_sfx);
	background_destroy();
	isoterrain_destroy(&g_terrain);
	texture_destroy(&g_cards_texture);
	texture_destroy(&g_entities_texture);
	texture_destroy(&g_ui_texture);
	shader_destroy(&g_sprite_shader);
	pipeline_destroy(&g_cards_pipeline);
	pipeline_destroy(&g_entities_pipeline);
	pipeline_destroy(&g_ui_pipeline);
	ecs_query_fini(g_ordered_handcards);
	ecs_fini(g_world);
}

static void update(struct scene_battle_s *scene, struct engine_s *engine, float dt) {
	const struct input_drag_s *drag = &(engine->input_drag);

	// pick up card
	if (drag->state == INPUT_DRAG_BEGIN) {
		g_selected_card = find_closest_handcard(drag->begin_x, drag->begin_y, 110.0f);

		if (g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card)) {
			c_handcard *hc = ecs_get_mut(g_world, g_selected_card, c_handcard);
			hc->hand_space = 0.4f;
			hc->is_selected = 1;
			ecs_modified(g_world, g_selected_card, c_handcard);

			Mix_PlayChannel(-1, g_pick_card_sfx, 0);
		}
	}

	if (drag->state == INPUT_DRAG_END) {
		// if card is selected: place/drop card
		if (g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card)) {
			c_handcard *hc = ecs_get_mut(g_world, g_selected_card, c_handcard);

			if (hc->can_be_placed) {
				on_game_event(EVENT_PLAY_CARD, &(event_info_t){ .entity = g_selected_card });
			} else {
				hc->hand_space = 1.0f;
				hc->is_selected = 0;
				ecs_modified(g_world, g_selected_card, c_handcard);
				g_selected_card = 0;
				Mix_PlayChannel(-1, g_slide_card_sfx, 0);
			}
		}
		else {
			// if not, assume movement swipe
			float swipe_angle;
			float swipe_dist;
			{
				vec2 begin = { drag->begin_x, drag->begin_y };
				vec2 end = { drag->end_x, drag->end_y };
				vec2 swipe_dir;
				glm_vec2_sub(end, begin, swipe_dir);
				swipe_dist = glm_vec2_norm(swipe_dir);
				glm_vec2_normalize(swipe_dir);
				swipe_angle = atan2f(swipe_dir[1], swipe_dir[0]);
			}

			if (swipe_dist > 10.0f) {
				enum {
					NE = 3,
					SE = 0,
					SW = 1,
					NW = 2,
				} segment = calculate_angle_segment(swipe_angle, 4);

				int flip_x = 0;
				int backside = 0;
				ecs_entity_t e = ecs_lookup(g_world, "player");
				switch (segment) {
				case NE:
					e = ecs_set(g_world, e, c_move_direction, { .x=0, .y=1, .z=0 });
					backside = 1;
					break;
				case SE:
					e = ecs_set(g_world, e, c_move_direction, { .x=1, .y=0, .z=0 });
					break;
				case SW:
					e = ecs_set(g_world, e, c_move_direction, { .x=0, .y=-1, .z=0 });
					flip_x = 1;
					break;
				case NW:
					e = ecs_set(g_world, e, c_move_direction, { .x=-1, .y=0, .z=0 });
					flip_x = 1;
					backside = 1;
					break;
				}

				c_tileset_tile *tile = ecs_get_mut(g_world, e, c_tileset_tile);
				tile->flip_x = flip_x;
				tile->backside = backside;
				ecs_modified(g_world, e, c_tileset_tile);

				g_next_turn = 1;
			}
		}
	}

	// move card
	if (drag->state == INPUT_DRAG_IN_PROGRESS && g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card)) {
		c_position *pos = ecs_get_mut(g_world, g_selected_card, c_position);
		c_handcard *hc = ecs_get_mut(g_world, g_selected_card, c_handcard);
		pos->x = drag->x;
		pos->y = drag->y;
		const int new_can_be_placed = (pos->y < g_engine->window_height - 128.0f);
		const int can_be_placed_changed = (new_can_be_placed != hc->can_be_placed);
		if (can_be_placed_changed) {
			hc->hand_space = new_can_be_placed ? 0.4f : 1.0f;
			hc->can_be_placed = new_can_be_placed;
			ecs_modified(g_world, g_selected_card, c_handcard);
		}
	}

	// add cards
	static float card_add_accum = -0.5f; // wait half a second before starting
	const float card_add_speed = 0.25f;
	card_add_accum += dt;
	if (card_add_accum >= card_add_speed) {
		card_add_accum -= card_add_speed;
		
		ecs_filter_t *filter = ecs_filter_init(g_world, &(ecs_filter_desc_t){
			.terms = {
				{ .id = ecs_id(c_card) },
				{ .id = ecs_id(c_position), .oper = EcsNot },
				{ .id = ecs_id(c_handcard), .oper = EcsNot },
			},
		});

		ecs_iter_t it = ecs_filter_iter(g_world, filter);
		while (ecs_filter_next(&it)) {
			if (it.count > 0) {
				ecs_entity_t e = it.entities[0];
				ecs_set(g_world, e, c_handcard, { .hand_space = 1.0f, .hand_target_pos = {0}, .is_selected = 0, .added_at_time=engine->time_elapsed });
				ecs_set(g_world, e, c_position, { .x = engine->window_width, .y = engine->window_height * 0.9f });
				Mix_PlayChannel(-1, g_place_card_sfx, 0);
			}
		}
		while (ecs_filter_next(&it)); // exhaust iterator. TODO: better way?
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
	const int t_padding = 40.0f;
	const float t_scale = ((engine->window_width - t_padding) / (float)g_terrain.projected_width);
	const int t_y = engine->window_height - g_terrain.projected_height * t_scale - 50.0f;

	glm_mat4_identity(engine->u_view);
	glm_translate_x(engine->u_view, (int)(t_padding * 0.5f));
	glm_translate_y(engine->u_view, (int)(t_y));
	glm_scale(engine->u_view, (float[]){t_scale, t_scale, t_scale});
	isoterrain_draw(&g_terrain, engine);

	// drawing systems
	pipeline_reset(&g_entities_pipeline);
	g_entities_pipeline.texture = &g_entities_texture;

	pipeline_reset(&g_cards_pipeline);
	g_cards_pipeline.texture = &g_cards_texture;

	if (g_next_turn) {
		g_next_turn = 0;
		ecs_run(g_world, ecs_id(system_turn_move_entities), engine->dt, NULL);
	}
	ecs_run(g_world, ecs_id(system_move_cards), engine->dt, NULL);
	ecs_run(g_world, ecs_id(system_draw_cards), engine->dt, NULL);
	ecs_run(g_world, ecs_id(system_draw_entities), engine->dt, NULL);

	pipeline_draw(&g_entities_pipeline, engine);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	pipeline_draw_ortho(&g_cards_pipeline, g_engine->window_width, g_engine->window_height);
	glDisable(GL_DEPTH_TEST);

	// draw ui
	pipeline_reset(&g_ui_pipeline);
	{
		drawcmd_t cmd = DRAWCMD_INIT;
		cmd.size.x = 96 - 16;
		cmd.size.y = 96 - 16;
		cmd.position.x = 8.0f;
		cmd.position.y = 8.0f;
		cmd.position.z = 0.0f;
		drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, ((int)engine->time_elapsed % 2) ? 0 : ((int)engine->time_elapsed % 7 == 0) ? 64 : 96, 112, 32, 32);
		pipeline_emit(&g_ui_pipeline, &cmd);

		cmd = DRAWCMD_INIT;
		cmd.size.x = 96;
		cmd.size.y = 96;
		cmd.position.x = 0.0f;
		cmd.position.y = 0.0f;
		cmd.position.z = 0.0f;
		drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, 0, 0, 96, 96);
		pipeline_emit(&g_ui_pipeline, &cmd);

		float hp = fabsf(cosf(engine->time_elapsed));
		cmd = DRAWCMD_INIT;
		cmd.size.x = ceilf(112.0f * hp);
		cmd.size.y = 16;
		cmd.position.x = -6;
		cmd.position.y = 90;
		drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, 96, 0, ceilf(112.0f * hp), 16);
		pipeline_emit(&g_ui_pipeline, &cmd);
	}
	pipeline_draw_ortho(&g_ui_pipeline, g_engine->window_width, g_engine->window_height);
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

static ecs_entity_t find_closest_handcard(float x, float y, float max_distance) {
	vec2 cursor_pos = {x, y};

	ecs_entity_t e = 0;
	float closest = glm_pow2(max_distance); // max distance 110px

	ecs_iter_t it = ecs_query_iter(g_world, g_ordered_handcards);
	while (ecs_query_next(&it)) {
		//c_card *cards = ecs_field(&it, c_card, 1);
		c_handcard *handcards = ecs_field(&it, c_handcard, 2);

		for (int i = 0; i < it.count; ++i) {
			float d2 = glm_vec2_distance2(handcards[i].hand_target_pos.raw, cursor_pos);
			if (d2 < closest) {
				closest = d2;
				e = it.entities[i];
			}
		}
	}

	return e;
}

static int order_handcards(ecs_entity_t e1, const void *data1, ecs_entity_t e2, const void *data2) {
	const c_handcard *c1 = data1;
	const c_handcard *c2 = data2;

	return (c1->added_at_time > c2->added_at_time) - (c1->added_at_time < c2->added_at_time);
}

static void on_game_event(enum event_type type, event_info_t *info) {
	switch (type) {
	case EVENT_PLAY_CARD:
		on_game_event_play_card(info);
		break;
	case EVENT_TYPE_MAX:
		assert(type != EVENT_TYPE_MAX);
		break;
	};
}

static void on_game_event_play_card(event_info_t *info) {
	assert(info != NULL);
	assert(info->entity != 0);
	assert(ecs_is_valid(g_world, info->entity));

	ecs_entity_t e = info->entity;

	c_card *card = ecs_get(g_world, e, c_card);
	printf("played card '%s'\n", card->name);

	// remove card
	ecs_delete(g_world, g_selected_card);
	g_selected_card = 0;
}

//
// systems implementation
//

static void system_turn_move_entities(ecs_iter_t *it) {
	c_blockpos *positions = ecs_field(it, c_blockpos, 1);
	c_move_direction *moves = ecs_field(it, c_move_direction, 2);
	
	for (int i = 0; i < it->count; ++i) {
		c_blockpos *pos = &positions[i];
		c_move_direction *move = &moves[i];
		assert(abs(move->x) <= 1);
		assert(abs(move->y) <= 1);
		assert(abs(move->z) <= 1);

		iso_block *next_surface = isoterrain_get_block(&g_terrain, pos->x + move->x, pos->y + move->y, pos->z + move->z - 1);
		iso_block *next_block = isoterrain_get_block(&g_terrain, pos->x + move->x, pos->y + move->y, pos->z + move->z);
		if (next_block && *next_block == BLOCK_AIR) {
			// slide on ice
			while (next_block != NULL && next_surface != NULL && *next_surface == BLOCK_ICE) {
				pos->x += move->x;
				pos->y += move->y;
				pos->z += move->z;
				next_surface = isoterrain_get_block(&g_terrain, pos->x + move->x, pos->y + move->y, pos->z + move->z - 1);
				next_block = isoterrain_get_block(&g_terrain, pos->x + move->x, pos->y + move->y, pos->z + move->z);
			}
			// normal move
			if (next_block != NULL || (next_block && *next_block == BLOCK_AIR)) {
				pos->x += move->x;
				pos->y += move->y;
				pos->z += move->z;
			}
		}

		ecs_remove(g_world, it->entities[i], c_move_direction);
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

static void system_draw_cards(ecs_iter_t *it) {
	c_card *cards = ecs_field(it, c_card, 1);
	c_handcard *handcards = NULL;
	c_position *positions = ecs_field(it, c_position, 3);

	if (ecs_field_is_set(it, 2)) {
		handcards = ecs_field(it, c_handcard, 2);
	}

	// TODO: this only works for a single archetype
	const int cards_count = it->count;

	float card_z = 0.0f;
	for (int i = 0; i < cards_count; ++i) {
		vec2s *card_pos = &positions[i];
		const float p = ((float)i / glm_max(cards_count - 1, 1));
		float angle = p * glm_rad(30.0f) - glm_rad(15.0f);

		int is_selected = (handcards && handcards[i].is_selected);
		int can_be_placed = (handcards && handcards[i].can_be_placed);
		if (is_selected) {
			angle = 0.0f;
		}
		float z_offset = is_selected * 0.1f;
		
		card_z += 0.01f;

		drawcmd_t cmd_card = DRAWCMD_INIT;
		cmd_card.size.x = 90;
		cmd_card.size.y = 128;
		if (can_be_placed) {
			angle = cosf(g_engine->time_elapsed * 18.0f) * 0.1f;
		}
		cmd_card.position.x = card_pos->x;
		cmd_card.position.y = card_pos->y;
		cmd_card.position.z = card_z + z_offset;
		cmd_card.angle = angle;
		cmd_card.position.x -= cmd_card.size.x * 0.5f;
		cmd_card.position.y -= cmd_card.size.y * 0.5f;
		drawcmd_t cmd_img = DRAWCMD_INIT;
		cmd_img.size.x = cmd_card.size.x;
		cmd_img.size.y = cmd_card.size.y * 0.5f;
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
		// icons
		for (int icon_i = 0; icon_i < cards[i].icon_ids_count; ++icon_i) {
			int icon_tex_x = cards[i].icon_ids[icon_i] % 2;
			int icon_tex_y = 4 + cards[i].icon_ids[icon_i] / 2.0f;
			drawcmd_t cmd_icon = DRAWCMD_INIT;
			cmd_icon.position.x = cmd_card.position.x + 7 + 12 * icon_i;
			cmd_icon.position.y = cmd_card.position.y - 6;
			cmd_icon.position.z = cmd_card.position.z;
			cmd_icon.size.x = cmd_icon.size.y = 20;
			cmd_icon.origin.x = cmd_icon.origin.y = 0.0f;
			cmd_icon.origin.z = 45 - 7 - 12 * icon_i;
			cmd_icon.origin.w = 64 + 6;
			cmd_icon.angle = cmd_card.angle;
			drawcmd_set_texture_subrect_tile(&cmd_icon, g_cards_pipeline.texture, 32, 32, icon_tex_x, icon_tex_y);
			pipeline_emit(&g_cards_pipeline, &cmd_icon);
		}

	}
}

static void system_draw_entities(ecs_iter_t *it) {
	c_blockpos *positions = ecs_field(it, c_blockpos, 1);
	c_tileset_tile *tiles = ecs_field(it, c_tileset_tile, 2);
	
	for (int i = 0; i < it->count; ++i) {
		vec2 screen_pos;
		c_blockpos blockpos = positions[i];
		isoterrain_pos_block_to_screen(&g_terrain, blockpos.x, blockpos.y, blockpos.z, screen_pos);

		c_tileset_tile *tile = &tiles[i];

		drawcmd_t cmd = DRAWCMD_INIT;
		cmd.size.x = 16;
		cmd.size.y = 16;
		glm_vec2(screen_pos, cmd.position.raw);
		cmd.position.y = g_terrain.projected_height - cmd.position.y;
		cmd.position.z = 0.0f;
		drawcmd_set_texture_subrect_tile(&cmd, g_entities_pipeline.texture,
			tile->tile_width, tile->tile_height,
			tile->tile_x, tile->tile_y + tile->backside);
		drawcmd_flip_texture_subrect(&cmd, tile->flip_x, 0);

		pipeline_emit(&g_entities_pipeline, &cmd);
		
	}
}

static void observer_on_update_handcards(ecs_iter_t *it) {
	c_card *changed_cards = ecs_field(it, c_card, 1);
	c_handcard *changed_handcards = ecs_field(it, c_handcard, 2);

	g_handcards_updated = 1;
}

