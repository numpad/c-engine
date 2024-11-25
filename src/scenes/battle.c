#include "battle.h"

#include <assert.h>
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
#include "engine.h"
#include "gl/texture.h"
#include "gl/shader.h"
#include "gl/graphics2d.h"
#include "gl/text.h"
#include "gl/model.h"
#include "game/background.h"
#include "gui/console.h"
#include "scenes/menu.h"
#include "util/util.h"

//
// structs & enums
//

enum event_type {
	EVENT_PLAY_CARD,
	EVENT_TYPE_MAX,
};

enum card_selection {
	CS_NOT_SELECTED = 0,
	CS_SELECTED,
	CS_SELECTED_INITIAL,
};

typedef struct event_info {
	ecs_entity_t entity;
} event_info_t;

struct camera {
	mat4 projection;
	mat4 view;
};

struct hexmap {
	int w, h;
	float tilesize;
	struct {short tile; short rotation;} *tiles;
	vec2s tile_offsets;
};

//
// private functions
//

static void recalculate_handcards(void);
static ecs_entity_t find_closest_handcard(float x, float y, float max_distance);
static int order_handcards(ecs_entity_t e1, const void *data1, ecs_entity_t e2, const void *data2);
static void on_game_event(enum event_type, event_info_t *);
static void on_game_event_play_card(event_info_t *info);

static void load_hextile_models(void);
static void hexmap_init(struct hexmap *);
static void hexmap_destroy(struct hexmap *);
static void hexmap_draw(struct hexmap *);

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
	enum card_selection is_selected;
	int   can_be_placed;
	float added_at_time;
} c_handcard;

ECS_COMPONENT_DECLARE(c_position);
ECS_COMPONENT_DECLARE(c_card);
ECS_COMPONENT_DECLARE(c_handcard);
ECS_COMPONENT_DECLARE(c_blockpos);

// queries
static ecs_query_t *g_ordered_handcards;
static int g_handcards_updated;

// systems
static void system_move_cards           (ecs_iter_t *);
static void system_draw_cards           (ecs_iter_t *);
static void observer_on_update_handcards(ecs_iter_t *);

ECS_SYSTEM_DECLARE(system_move_cards);
ECS_SYSTEM_DECLARE(system_draw_cards);

//
// vars
//
static engine_t     *g_engine;

// game state
static texture_t     g_cards_texture;
static texture_t     g_ui_texture;
static shader_t      g_sprite_shader;
static shader_t      g_text_shader;
static pipeline_t    g_cards_pipeline;
static pipeline_t    g_ui_pipeline;
static pipeline_t    g_text_pipeline;
static ecs_world_t  *g_world;
static ecs_entity_t  g_selected_card;
static fontatlas_t   g_card_font;
static model_t       g_model;
static model_t       g_fun_models[3];
static model_t       g_hextiles[6];
static float         g_pickup_next_card;
static struct camera g_camera;
static struct hexmap g_hexmap;
static vec3s         g_character_position;

// testing
static Mix_Chunk    *g_place_card_sfx;
static Mix_Chunk    *g_pick_card_sfx;
static Mix_Chunk    *g_slide_card_sfx;


//
// scene functions
//

static void load(struct scene_battle_s *battle, struct engine_s *engine) {
	g_engine = engine;
	g_handcards_updated = 0;
	g_selected_card = 0;
	g_pickup_next_card = -0.75f; // wait 0.75s before spawning.
	console_log(engine, "Starting battle scene!");

	// initialize camera
	glm_mat4_identity(g_camera.view);
	glm_translate(g_camera.view, (vec3){ 0.0f, 0.0f, -1000.0f });
	glm_rotate_x(g_camera.view, glm_rad(35.0f), g_camera.view);
	glm_mat4_identity(g_camera.projection);

	static int loads = 0;
	const char *models[] = {"res/models/characters/Knight.glb", "res/models/characters/Mage.glb", "res/models/characters/Barbarian.glb", "res/models/characters/Rogue.glb"};
	int load_error = model_init_from_file(&g_model, models[loads++ % 4]);
	assert(load_error == 0);

	// some random props
	const char *fun_models[] = {
		"res/models/decoration/props/bucket_water.gltf",
		"res/models/decoration/props/target.gltf",
		"res/models/decoration/props/crate_A_big.gltf",
	};
	for (uint i = 0; i < count_of(fun_models); ++i) {
		int load_fun_error = model_init_from_file(&g_fun_models[i], fun_models[i]);
		assert(load_fun_error == 0);
	}

	load_hextile_models();
	hexmap_init(&g_hexmap);
	g_character_position = (vec3s){ .x=200.0f, .y=0.0f, .z=300.0f };

	// ecs
	g_world = ecs_init();
	ECS_COMPONENT_DEFINE(g_world, c_position);
	ECS_COMPONENT_DEFINE(g_world, c_card);
	ECS_COMPONENT_DEFINE(g_world, c_handcard);
	ECS_SYSTEM_DEFINE(g_world, system_move_cards, 0, c_position, c_handcard);
	ECS_SYSTEM_DEFINE(g_world, system_draw_cards, 0, c_card, ?c_handcard, c_position); _syntax_fix_label:

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

	// ui
	{
		struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
		texture_init_from_image(&g_ui_texture, "res/image/ui.png", &settings);
		pipeline_init(&g_ui_pipeline, &g_sprite_shader, 128);
		g_ui_pipeline.texture = &g_ui_texture;
	}

	// text rendering
	{
		fontatlas_init(&g_card_font, engine->freetype);
		fontatlas_add_face(&g_card_font, "res/font/NotoSans-Regular.ttf",    11);
		fontatlas_add_face(&g_card_font, "res/font/NotoSans-Bold.ttf",       11);
		fontatlas_add_face(&g_card_font, "res/font/NotoSans-Italic.ttf",     11);
		fontatlas_add_face(&g_card_font, "res/font/NotoSans-BoldItalic.ttf", 11);
		// printable ascii characters
		fontatlas_add_ascii_glyphs(&g_card_font);

		shader_init_from_dir(&g_text_shader, "res/shader/text/");
		pipeline_init(&g_text_pipeline, &g_text_shader, 2048);
		g_text_pipeline.texture = &g_card_font.texture_atlas;
		pipeline_reset(&g_text_pipeline);
		fontatlas_writef(&g_card_font, &g_text_pipeline, "   $0CARD$0 $B$1T$2I$3T$1L$2E$0");
	}

	// background
	background_set_parallax("res/image/bg-glaciers/%d.png", 4);
	background_set_parallax_offset(-0.4f);

	// audio
	g_place_card_sfx = Mix_LoadWAV("res/sounds/place_card.ogg");
	g_pick_card_sfx = Mix_LoadWAV("res/sounds/cardSlide5.ogg");
	g_slide_card_sfx = Mix_LoadWAV("res/sounds/cardSlide7.ogg");
}


static void destroy(struct scene_battle_s *battle, struct engine_s *engine) {
	Mix_FreeChunk(g_place_card_sfx);
	Mix_FreeChunk(g_pick_card_sfx);
	Mix_FreeChunk(g_slide_card_sfx);

	background_destroy();
	hexmap_destroy(&g_hexmap);

	texture_destroy(&g_cards_texture);
	texture_destroy(&g_ui_texture);

	shader_destroy(&g_sprite_shader);
	shader_destroy(&g_text_shader);

	pipeline_destroy(&g_cards_pipeline);
	pipeline_destroy(&g_ui_pipeline);
	pipeline_destroy(&g_text_pipeline);

	ecs_query_fini(g_ordered_handcards);
	ecs_fini(g_world);

	model_destroy(&g_model);
}


static void update(struct scene_battle_s *battle, struct engine_s *engine, float dt) {
	const struct input_drag_s *drag = &(engine->input_drag);

	// pick up card
	if (drag->state == INPUT_DRAG_BEGIN) {
		g_selected_card = find_closest_handcard(drag->begin_x, drag->begin_y, 110.0f);

		if (g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card)) {
			c_handcard *hc = ecs_get_mut(g_world, g_selected_card, c_handcard);
			hc->hand_space = 0.4f;
			hc->is_selected = CS_SELECTED_INITIAL;
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
				hc->is_selected = CS_NOT_SELECTED;
				ecs_modified(g_world, g_selected_card, c_handcard);
				g_selected_card = 0;
				Mix_PlayChannel(-1, g_slide_card_sfx, 0);
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
			hc->is_selected = CS_SELECTED;
			hc->hand_space = new_can_be_placed ? 0.4f : 1.0f;
			hc->can_be_placed = new_can_be_placed;
			ecs_modified(g_world, g_selected_card, c_handcard);
		}
	}

	// add cards
	const float card_add_speed = 0.25f;
	g_pickup_next_card += dt;
	if (g_pickup_next_card >= card_add_speed) {
		g_pickup_next_card -= card_add_speed;
		
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
				ecs_set(g_world, e, c_handcard, { .hand_space = 1.0f, .hand_target_pos = {0}, .is_selected = CS_NOT_SELECTED, .added_at_time=engine->time_elapsed });
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


static void draw(struct scene_battle_s *battle, struct engine_s *engine) {
	engine_set_clear_color(0.34f, 0.72f, 0.98f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// TODO: Update camera, do this on window resize instead.
	float size = engine->window_width;
	glm_ortho(-size, size, -size * g_engine->window_aspect, size * g_engine->window_aspect, 1.0f, 2000.0f, g_camera.projection);
	//glm_perspective(glm_rad(100.0f), g_engine->window_width / (float)g_engine->window_height, 1.0f, 2000.0f, g_camera.projection);

	// draw background
	background_draw(engine);

	// draw terrain
	hexmap_draw(&g_hexmap);

	// drawing systems
	pipeline_reset(&g_cards_pipeline);
	g_cards_pipeline.texture = &g_cards_texture;

	// Player model
	{
		if (engine->input_drag.state == INPUT_DRAG_IN_PROGRESS) {
			g_character_position = screen_to_world(
				engine->window_width, engine->window_height, g_camera.projection, g_camera.view,
				engine->input_drag.x, engine->input_drag.y);
		}

		// calculate matrices
		mat4 model = GLM_MAT4_IDENTITY_INIT;
		//glm_translate(model, (vec3){ 100.0f, 0.0f, sqrtf(3.0f) * 200.0f });
		glm_translate(model, g_character_position.raw);
		glm_rotate_y(model, glm_rad(sinf(g_engine->time_elapsed * 2.0f) * 80.0f), model);
		const float scale = 80.0f;
		glm_scale(model, (vec3){ scale, scale, scale });

		// TODO: also do this for every node, as the model matrix changes...
		mat4 modelView = GLM_MAT4_IDENTITY_INIT;
		glm_mat4_mul(g_camera.view, model, modelView);
		mat3 normalMatrix = GLM_MAT3_IDENTITY_INIT;
		glm_mat4_pick3(modelView, normalMatrix);
		glm_mat3_inv(normalMatrix, normalMatrix);
		glm_mat3_transpose(normalMatrix);

		// draw
		glEnable(GL_DEPTH_TEST);

		// Draw character
		shader_set_uniform_mat3(&g_model.shader, "u_normalMatrix", (float*)normalMatrix);
		model_draw(&g_model, g_camera.projection, g_camera.view, model);

		rng_seed(123);
		for (int i = 0; i < 50; ++i) {
			int c = (i % 3);
			float t = engine->time_elapsed * 0.6f * (1+c) + i * 0.14f;
			shader_set_uniform_mat3(&g_fun_models[c].shader, "u_normalMatrix", (float*)normalMatrix);
			glm_mat4_identity(model);
			glm_translate(model, (vec3){ cosf(t) * (200.0f + c*100.0f), -170.0f + sinf(t) * (200.0f + c*100.0f) + 70.0f * c, -460.0f + c * 150.0f});
			float ay = rng_f();
			float az = rng_f();
			vec3 axis = {0.0f, ay, az};
			glm_normalize(axis);
			glm_rotate(model, glm_rad(fmodf(engine->time_elapsed * (45.0f * (rng_f() + 0.5f)) + rng_f() * 360.0f, 360.0f)), axis);
			float s = 200.0f + rng_f() * 300.0f;
			glm_scale(model, (vec3){ s, s, s });
			model_draw(&g_fun_models[c], g_camera.projection, g_camera.view, model);
		}

		// Portrait model
		mat4 portrait_view = GLM_MAT4_IDENTITY_INIT;
		glm_mat4_identity(model);
		glm_translate(model, (vec3){ engine->window_width - 100.0f, engine->window_height - 240.0f, -300.0f });
		glm_rotate_x(model, glm_rad(10.0f + cosf(g_engine->time_elapsed) * 10.0f), model);
		glm_rotate_y(model, glm_rad(sinf(g_engine->time_elapsed) * 40.0f), model);
		glm_scale(model, (vec3){ 75.0f, 75.0f, 75.0f});
		const float pr = engine->window_pixel_ratio;
		glEnable(GL_SCISSOR_TEST);
		glScissor(engine->window_width * pr - 85.0f * pr, engine->window_height * pr - 85.0f * pr, 66 * pr, 66 * pr);
		model_draw(&g_model, g_camera.projection, portrait_view, model);
		glDisable(GL_SCISSOR_TEST);

		glDisable(GL_DEPTH_TEST);
	}

	// draw cards
	ecs_run(g_world, ecs_id(system_move_cards), engine->dt, NULL);
	ecs_run(g_world, ecs_id(system_draw_cards), engine->dt, NULL);

	// draw ui
	pipeline_reset(&g_ui_pipeline);
	{
		drawcmd_t cmd;
		
		// Frame
		cmd = DRAWCMD_INIT;
		cmd.size.x = 96;
		cmd.size.y = 96;
		cmd.position.x = engine->window_width - 96 - 4;
		cmd.position.y = 4;
		cmd.position.z = -0.9f;
		drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, 0, 0, 32, 32);
		pipeline_emit(&g_ui_pipeline, &cmd);

		// Healthbar frame
		cmd = DRAWCMD_INIT;
		cmd.size.x = 48;
		cmd.size.y = 192;
		cmd.position.x = 1;
		cmd.position.y = 4;
		drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, 32, 0, 16, 64);
		pipeline_emit(&g_ui_pipeline, &cmd);

		// Healthbar fill
		int hp = 64.0f * fabsf(cosf(engine->time_elapsed));
		cmd = DRAWCMD_INIT;
		cmd.size.x = 48;
		cmd.size.y = hp * 3.0f;
		cmd.position.x = 1;
		cmd.position.y = 4;
		cmd.position.z = 0.1f;
		drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, 48, 0, 16, hp);
		pipeline_emit(&g_ui_pipeline, &cmd);
	}

	glEnable(GL_DEPTH_TEST);
	pipeline_draw_ortho(&g_ui_pipeline, g_engine->window_width, g_engine->window_height);
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

	const c_card *card = ecs_get(g_world, e, c_card);
	console_log(g_engine, "Played card: \"%s\"...", card->name);

	// remove card
	ecs_delete(g_world, g_selected_card);
	g_selected_card = 0;
}

static void load_hextile_models(void) {
	const char *models[] = {
		"res/models/tiles/base/hex_grass.gltf",
		"res/models/tiles/base/hex_water.gltf",
		"res/models/tiles/coast/hex_coast_A.gltf",
		"res/models/tiles/coast/hex_coast_B.gltf",
		"res/models/tiles/coast/hex_coast_C.gltf",
		"res/models/tiles/coast/hex_coast_D.gltf",
		"res/models/tiles/coast/hex_coast_E.gltf",
		"res/models/tiles/roads/hex_road_A.gltf",
		"res/models/tiles/roads/hex_road_B.gltf",
		"res/models/tiles/roads/hex_road_E.gltf",
	};

	for (uint i = 0; i < count_of(models); ++i) {
		int err = model_init_from_file(&g_hextiles[i], models[i]);
		assert(err == 0);
	}
}
static void hexmap_init(struct hexmap *map) {
	map->w = 7;
	map->h = 9;
	map->tilesize = 115.0f;
	map->tiles = calloc(map->w * map->h, sizeof(*map->tiles));

	// Make some map
#define M(x, y, T, R) map->tiles[x + map->w * y].tile = T; map->tiles[x + map->w * y].rotation = R;
	M(0, 0, 1,  0);
	M(1, 0, 4, -1);
	M(2, 0, 6, -1);
	M(0, 1, 1,  0);
	M(1, 1, 1,  0);
	M(2, 1, 4, -2);
	M(0, 2, 1,  0);
	M(1, 2, 5, -3);
	M(2, 2, 6, -2);
	M(0, 3, 3, -4);
	M(1, 3, 4, -3);
	M(2, 3, 6, -2);
	M(0, 4, 6, -3);

	M(4, 0, 8,  1);
	M(4, 1, 8, -2);
	M(4, 2, 9, -1);
	M(5, 2, 7,  0);
	M(6, 2, 7,  0);
	M(5, 3, 8,  1);
	M(4, 4, 7, -2);
	M(4, 5, 8, -2);
	M(4, 6, 8,  1);
	M(4, 7, 8, -2);
	M(4, 8, 7, -1);
#undef M

	// precomputed
	map->tile_offsets = (vec2s){
		.x = sqrtf(3.f) * map->tilesize,
		.y = (3.f / 2.f) * map->tilesize,
	};
}

static void hexmap_destroy(struct hexmap *map) {
	free(map->tiles);
}

static void hexmap_draw(struct hexmap *map) {
	glEnable(GL_DEPTH_TEST);

	float horiz = map->tile_offsets.x;
	float vert = map->tile_offsets.y;
	int n_tiles = map->w * map->h;

	for (int i = 0; i < n_tiles; ++i) {
		int q = i % map->w;
		int r = floorf(i / (float)map->w);
		float horiz_offset = (r % 2 == 0) ? horiz * 0.5f : 0.0f;

		vec2s pos = {
			.x = q * horiz + horiz_offset,
			.y = r * vert,
		};

		mat4 model = GLM_MAT4_IDENTITY_INIT;
		glm_translate(model, (vec3){ -horiz * 3.f, 0.0f, vert * -2.0f });
		glm_translate(model, (vec3){ pos.x, 0.0f, pos.y });
		glm_rotate_y(model, map->tiles[i].rotation * glm_rad(60.0f), model);
		glm_scale(model, (vec3){ 100.0f, 100.0f, 100.0f});

		mat4 modelView = GLM_MAT4_IDENTITY_INIT;
		glm_mat4_mul(g_camera.view, model, modelView);
		mat3 normalMatrix = GLM_MAT3_IDENTITY_INIT;
		glm_mat4_pick3(modelView, normalMatrix);
		glm_mat3_inv(normalMatrix, normalMatrix);
		glm_mat3_transpose(normalMatrix);

		shader_set_uniform_mat3(&g_hextiles[0].shader, "u_normalMatrix", (float*)normalMatrix);
		model_draw(&g_hextiles[map->tiles[i].tile], g_camera.projection, g_camera.view, model);
	}
	glDisable(GL_DEPTH_TEST);
}


//
// systems implementation
//

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

		int is_selected = (handcards && handcards[i].is_selected == CS_SELECTED);
		int can_be_placed = (handcards && handcards[i].can_be_placed);
		if (is_selected) {
			angle = 0.0f;
		}
		float z_offset = is_selected * 0.1f;
		card_z += 0.01f;

		float extra_scale = 1.0f;
		if (handcards && handcards[i].is_selected == CS_SELECTED_INITIAL) {
			angle = 0.0f;
			card_pos->x = g_engine->window_width * 0.5f;
			card_pos->y = g_engine->window_height * 0.5f;
			extra_scale = 3.0f;
		}

		drawcmd_t cmd_card = DRAWCMD_INIT;
		cmd_card.size.x = 90 * extra_scale;
		cmd_card.size.y = 128 * extra_scale;
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
		drawcmd_set_texture_subrect(&cmd_card, g_cards_pipeline.texture, 2, 226, 359, 512);

		pipeline_emit(&g_cards_pipeline, &cmd_card);
		// icons
		for (int icon_i = 0; icon_i < cards[i].icon_ids_count; ++icon_i) {
			int icon_tex_x = cards[i].icon_ids[icon_i] % 2;
			int icon_tex_y = 4 + cards[i].icon_ids[icon_i] / 2.0f;
			drawcmd_t cmd_icon = DRAWCMD_INIT;
			cmd_icon.position.x = cmd_card.position.x + 7 + 12 * extra_scale * icon_i;
			cmd_icon.position.y = cmd_card.position.y - 6 * extra_scale;
			cmd_icon.position.z = cmd_card.position.z;
			cmd_icon.size.x = cmd_icon.size.y = 20 * extra_scale;
			cmd_icon.origin.x = cmd_icon.origin.y = 0.0f;
			cmd_icon.origin.z = 45 - 7 - 12 * icon_i;
			cmd_icon.origin.w = 64 + 6;
			cmd_icon.angle = cmd_card.angle;
			drawcmd_set_texture_subrect_tile(&cmd_icon, g_cards_pipeline.texture, 32, 32, icon_tex_x, icon_tex_y);
			pipeline_emit(&g_cards_pipeline, &cmd_icon);
		}
		// Flush / draw immediately instead of batching...
		// This is needed so that text appears on top of the card, and isnt drawn below.
		pipeline_draw_ortho(&g_cards_pipeline, g_engine->window_width, g_engine->window_height);
		pipeline_reset(&g_cards_pipeline);

		// write text
		mat4 model = GLM_MAT4_IDENTITY_INIT;
		glm_translate(model, cmd_card.position.raw);
		glm_rotate_at(model,
			(vec3){
				cmd_card.size.x * 0.5f,
				cmd_card.size.y * 0.5f,
				0
			},
			cmd_card.angle, (vec3){0.0f, 0.0f, 1.0f}
		);
		glm_translate(model, (vec3){5, 64 * extra_scale + 11 + 5, 0});
		pipeline_set_transform(&g_text_pipeline, model);
		pipeline_draw_ortho(&g_text_pipeline, g_engine->window_width, g_engine->window_height);
	}
}


static void observer_on_update_handcards(ecs_iter_t *it) {
	c_card *changed_cards = ecs_field(it, c_card, 1);
	c_handcard *changed_handcards = ecs_field(it, c_handcard, 2);

	g_handcards_updated = 1;
}

