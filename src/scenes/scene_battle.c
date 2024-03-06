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

// general information about a card
typedef struct {
	char *name;
	int image_id;
} c_card ;

// a cards state when held in hand
typedef struct {
	vec2s hand_target_pos;
	float hand_space;
} c_handcard;

// position on the isoterrain grid
typedef struct {
	int x, y, z;
} c_blockpos;

ECS_COMPONENT_DECLARE(c_card);
ECS_COMPONENT_DECLARE(c_handcard);
ECS_COMPONENT_DECLARE(c_blockpos);

// queries
static ecs_query_t *g_ordered_handcards;
static int g_handcards_updated;

// systems
static void system_draw_cards(ecs_iter_t *);
static void observer_on_update_handcards(ecs_iter_t *);

ECS_SYSTEM_DECLARE(system_draw_cards);

//
// vars
//
static engine_t *g_engine;

// game state
struct isoterrain_s *g_terrain;
static texture_t g_cards_texture;
static shader_t g_cards_shader;
static pipeline_t g_cards_pipeline;

static ecs_world_t *g_world;
static int g_cards_to_add = 0;

// testing
static Mix_Chunk *sound;


//
// scene functions
//

static void load(struct scene_battle_s *scene, struct engine_s *engine) {
	g_engine = engine;
	g_cards_to_add = 5;
	g_handcards_updated = 0;

	// ecs
	g_world = ecs_init();
	ECS_COMPONENT_DEFINE(g_world, c_card);
	ECS_COMPONENT_DEFINE(g_world, c_handcard);
	ECS_COMPONENT_DEFINE(g_world, c_blockpos);
	ECS_SYSTEM_DEFINE(g_world, system_draw_cards, 0, c_card, c_handcard);

	g_ordered_handcards = ecs_query(g_world, {
		.filter.terms = { {ecs_id(c_card)}, {ecs_id(c_handcard)} },
		//.order_by_component = ecs_id(c_handcard),
		//.order_by = order_handcards,
		});
	
	ecs_observer(g_world, {
		.filter.terms = { {ecs_id(c_card)}, {ecs_id(c_handcard)}, },
		.events = { EcsOnAdd, EcsOnRemove },
		.callback = observer_on_update_handcards,
		});

	// add some debug cards
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Attack", 0 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Attack", 0 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Fire Spell", 4 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Defend", 2 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Meal", 1 });
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_card, { "Corruption", 5 });
	}

	// add some debug entities
	{
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_blockpos, { 8, 4, 2 });
	}

	// isoterrain
	g_terrain = malloc(sizeof(struct isoterrain_s));
	isoterrain_init_from_file(g_terrain, "res/data/levels/winter.json");

	// card renderer
	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	settings.filter_min = GL_LINEAR;
	settings.filter_mag = GL_LINEAR;
	texture_init_from_image(&g_cards_texture, "res/image/cards.png", &settings);
	shader_init_from_dir(&g_cards_shader, "res/shader/sprite/");

	pipeline_init(&g_cards_pipeline, &g_cards_shader, 128);

	// background
	background_set_parallax("res/image/bg-glaciers/%d.png", 4);
	background_set_parallax_offset(-0.4f);

	// audio
	sound = Mix_LoadWAV("res/sounds/test.wav");
}

static void destroy(struct scene_battle_s *scene, struct engine_s *engine) {
	ecs_query_fini(g_ordered_handcards);
	background_destroy();
	isoterrain_destroy(g_terrain);
	free(g_terrain);
	texture_destroy(&g_cards_texture);
	shader_destroy(&g_cards_shader);
	pipeline_destroy(&g_cards_pipeline);
	ecs_fini(g_world);
}

static void update(struct scene_battle_s *scene, struct engine_s *engine, float dt) {
	const struct input_drag_s *drag = &(engine->input_drag);

	// add cards
	static float card_add_accum = 0.0f;
	card_add_accum += dt;
	if (card_add_accum >= 0.5f) {
		card_add_accum -= 0.5f;
		
		ecs_filter_t *filter = ecs_filter_init(g_world, &(ecs_filter_desc_t){
			.terms = {
				{ .id = ecs_id(c_card) },
				{ .id = ecs_id(c_handcard), .oper = EcsNot }
			},
		});

		ecs_iter_t it = ecs_filter_iter(g_world, filter);
		if (ecs_filter_next(&it)) {
			ecs_set(g_world, it.entities[0], c_handcard, { .hand_space = 1.0f, .hand_target_pos = {0} });
		}
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

	// draw cards
	ecs_run(g_world, ecs_id(system_draw_cards), engine->dt, NULL);
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

// systems

static void system_draw_cards(ecs_iter_t *it) {
	glm_mat4_identity(g_engine->u_view);
	pipeline_reset(&g_cards_pipeline);
	g_cards_pipeline.texture = &g_cards_texture;
	
	c_card *cards = ecs_field(it, c_card, 1);
	c_handcard *handcards = ecs_field(it, c_handcard, 2);

	// TODO: this only works for a single archetype
	const int cards_count = it->count;

	float card_z = 0.0f;
	for (int i = 0; i < cards_count; ++i) {
		const float p = ((float)i / glm_max(cards_count - 1, 1));
		const float angle = p * glm_rad(30.0f) - glm_rad(15.0f);
		float z_offset = 0.0f;
		
		card_z += 0.01f;

		drawcmd_t cmd_card = DRAWCMD_INIT;
		cmd_card.size.x = 90;
		cmd_card.size.y = 128;
		cmd_card.position.x = handcards[i].hand_target_pos.x; //x;
		cmd_card.position.y = handcards[i].hand_target_pos.y; //y;
		cmd_card.angle = angle;
		cmd_card.position.x -= cmd_card.size.x * 0.5f;
		cmd_card.position.y -= cmd_card.size.y * 0.5f;
		cmd_card.position.z = card_z + z_offset;
		drawcmd_t cmd_img = DRAWCMD_INIT;
		cmd_img.size.x = 90;
		cmd_img.size.y = 64;
		glm_vec3_dup(cmd_card.position.raw, cmd_img.position.raw);
		cmd_img.angle = cmd_card.angle;
		cmd_img.position.x = cmd_card.position.x;
		cmd_img.position.y = cmd_card.position.y;
		cmd_img.position.z = cmd_card.position.z;
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

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	pipeline_draw(&g_cards_pipeline, g_engine);
	glDisable(GL_DEPTH_TEST);
}

static void observer_on_update_handcards(ecs_iter_t *it) {
	c_card *changed_cards = ecs_field(it, c_card, 1);
	c_handcard *changed_handcards = ecs_field(it, c_handcard, 2);

	if (it->event == EcsOnAdd) {
		g_handcards_updated = 1;
	}
}

