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
#include "gl/gbuffer.h"
#include "gl/camera.h"
#include "game/background.h"
#include "game/hexmap.h"
#include "gui/console.h"
#include "scenes/menu.h"
#include "util/util.h"
#include "game/pathfinder.h"

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

enum gamestate_battle {
	GS_BATTLE_BEGIN    ,
	GS_ROUND_BEGIN     ,
	GS_TURN_BEGIN      , GS_TURN_PLAYER_BEGIN,
	GS_TURN_IN_PROGRESS, GS_TURN_PLAYER_IN_PROGRESS,
	GS_TURN_END        , GS_TURN_PLAYER_END,
	GS_ROUND_END       ,
	GS_BATTLE_END      ,
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
static void draw_ui(pipeline_t *pipeline);
static void on_game_event(enum event_type, event_info_t *);
static void on_game_event_play_card(event_info_t *info);
static int count_handcards(void);
static void interact_with_handcards(struct input_drag_s *drag);
static int add_cards_to_hand(float dt);
static void spawn_random_card(void);
static void update_gamestate(enum gamestate_battle state, float dt);
static int gamestate_changed(enum gamestate_battle old_state, enum gamestate_battle new_state);

//
// ecs
//

// components

// pos
typedef vec2s c_pos2d;
typedef vec3s c_pos3d;

typedef struct {
	int model_index;
	float scale;
} c_model;

typedef struct {
	vec3s vel;
} c_velocity;

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

ECS_COMPONENT_DECLARE(c_pos2d);
ECS_COMPONENT_DECLARE(c_pos3d);
ECS_COMPONENT_DECLARE(c_card);
ECS_COMPONENT_DECLARE(c_handcard);
ECS_COMPONENT_DECLARE(c_model);
ECS_COMPONENT_DECLARE(c_velocity);

// queries
static ecs_query_t *g_ordered_handcards;
static int g_handcards_updated;

// systems
static void system_move_cards           (ecs_iter_t *);
static void system_move_models          (ecs_iter_t *);
static void system_draw_cards           (ecs_iter_t *);
static void system_draw_models          (ecs_iter_t *);
static void observer_on_update_handcards(ecs_iter_t *);

ECS_SYSTEM_DECLARE(system_move_cards);
ECS_SYSTEM_DECLARE(system_move_models);
ECS_SYSTEM_DECLARE(system_draw_cards);
ECS_SYSTEM_DECLARE(system_draw_models);


//
// vars
//
static struct engine *g_engine;
static struct gbuffer g_gbuffer;

// game state
static texture_t             g_cards_texture;
static texture_t             g_ui_texture;
static shader_t              g_sprite_shader;
static shader_t              g_text_shader;
static pipeline_t            g_cards_pipeline;
static pipeline_t            g_ui_pipeline;
static pipeline_t            g_text_pipeline;
static ecs_world_t          *g_world;
static ecs_entity_t          g_selected_card;
static fontatlas_t           g_card_font;
static model_t               g_player_model;
static model_t               g_enemy_model;
static model_t               g_props_model[4];
static model_t               g_hextiles[6];
static float                 g_pickup_next_card;
static struct camera         g_camera;
static struct camera         g_portrait_camera;
static struct hexmap         g_hexmap;
static vec3s                 g_character_position;
static enum gamestate_battle g_gamestate;
static enum gamestate_battle g_next_gamestate;
static struct { float x, y, w, h; } g_button_end_turn;
static struct hexcoord       g_move_goal;


// testing
static Mix_Chunk    *g_place_card_sfx;
static Mix_Chunk    *g_pick_card_sfx;
static Mix_Chunk    *g_slide_card_sfx;


//
// scene functions
//

static void load(struct scene_battle_s *battle, struct engine *engine) {
	g_engine = engine;
	g_handcards_updated = 0;
	g_selected_card = 0;
	g_pickup_next_card = -0.75f; // wait 0.75s before spawning.
	g_gamestate = g_next_gamestate = GS_BATTLE_BEGIN;

	g_button_end_turn.x = g_engine->window_width - 150.0f;
	g_button_end_turn.y = g_engine->window_height - 200.0f;
	g_button_end_turn.w = 130.0f;
	g_button_end_turn.h = 60.0f;

	console_log(engine, "Starting battle scene!");
	gbuffer_init(&g_gbuffer, engine);

	static int loads = 0;
	const char *models[] = {"res/models/characters/Knight.glb", "res/models/characters/Mage.glb", "res/models/characters/Barbarian.glb", "res/models/characters/Rogue.glb"};
	int load_error = model_init_from_file(&g_player_model, models[loads++ % 4]);
	assert(load_error == 0);
	load_error = model_init_from_file(&g_enemy_model, "res/models/characters/Skeleton_Minion.glb");
	assert(load_error == 0);

	// some random props
	const char *fun_models[] = {
		"res/models/decoration/props/bucket_water.gltf",
		"res/models/decoration/props/target.gltf",
		"res/models/decoration/props/crate_A_big.gltf",
		"res/models/survival/campfire-pit.glb",
	};
	for (uint i = 0; i < count_of(fun_models); ++i) {
		int load_fun_error = model_init_from_file(&g_props_model[i], fun_models[i]);
		assert(load_fun_error == 0);
	}

	hexmap_init(&g_hexmap);
	vec2s p = hexmap_coord_to_world_position(&g_hexmap, 2, 5);
	g_character_position = (vec3s){ .x=p.x, .y=0.0f, .z=p.y };

	// initialize camera
	camera_init_default(&g_camera, engine->window_width, engine->window_height);
	glm_translate(g_camera.view, (vec3){ -g_hexmap.tile_offsets.x * 3.25f, 0.0f, g_hexmap.tile_offsets.y * -3.0f });
	camera_init_default(&g_portrait_camera, engine->window_width, engine->window_height);
	glm_look((vec3){ 0.0f, 0.0f, 100.0f }, (vec3){ 0.0f, 0.0f, -1.0f }, (vec3){ 0.0f, 1.0f, 0.0f }, (float*)&g_portrait_camera.view);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// ecs
	g_world = ecs_init();
	ECS_COMPONENT_DEFINE(g_world, c_pos2d);
	ECS_COMPONENT_DEFINE(g_world, c_pos3d);
	ECS_COMPONENT_DEFINE(g_world, c_card);
	ECS_COMPONENT_DEFINE(g_world, c_handcard);
	ECS_COMPONENT_DEFINE(g_world, c_model);
	ECS_COMPONENT_DEFINE(g_world, c_velocity);
	ECS_SYSTEM_DEFINE(g_world, system_move_cards, 0, c_pos2d, c_handcard);
	ECS_SYSTEM_DEFINE(g_world, system_move_models, 0, c_pos3d, c_model, c_velocity);
	ECS_SYSTEM_DEFINE(g_world, system_draw_cards, 0, c_card, ?c_handcard, c_pos2d); _syntax_fix_label:
	ECS_SYSTEM_DEFINE(g_world, system_draw_models, 0, c_pos3d, c_model);

	g_ordered_handcards = ecs_query(g_world, {
		.filter.terms = { {ecs_id(c_card)}, {ecs_id(c_handcard)} },
		.order_by_component = ecs_id(c_handcard),
		.order_by = order_handcards,
		});
	
	ecs_observer(g_world, {
		.filter.terms = { {ecs_id(c_card)}, {ecs_id(c_handcard)}, {ecs_id(c_pos2d)}, },
		.events = { EcsOnAdd, EcsOnRemove, EcsOnSet },
		.callback = observer_on_update_handcards,
		});

	// Add some models
	{
		vec2s p = hexmap_index_to_world_position(&g_hexmap, 3 + 4 * g_hexmap.w);
		ecs_entity_t e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_pos3d, { .x=p.x, .y=0.0f, .z=p.y });
		ecs_set(g_world, e, c_model, { .model_index=3, .scale=450.0f });
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
	background_set_parallax("res/image/bg-clouds/%d.png", 4);
	background_set_parallax_offset(-0.7f);

	// audio
	g_place_card_sfx = Mix_LoadWAV("res/sounds/place_card.ogg");
	g_pick_card_sfx = Mix_LoadWAV("res/sounds/cardSlide5.ogg");
	g_slide_card_sfx = Mix_LoadWAV("res/sounds/cardSlide7.ogg");
}


static void destroy(struct scene_battle_s *battle, struct engine *engine) {
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

	model_destroy(&g_player_model);
	model_destroy(&g_enemy_model);
	gbuffer_destroy(&g_gbuffer);
}

static void update(struct scene_battle_s *battle, struct engine *engine, float dt) {
	interact_with_handcards(&engine->input_drag);
	if (g_handcards_updated) {
		g_handcards_updated = 0;
		recalculate_handcards();
	}

	update_gamestate(g_gamestate, dt);

	ecs_run(g_world, ecs_id(system_move_cards), engine->dt, NULL);
	ecs_run(g_world, ecs_id(system_move_models), engine->dt, NULL);

	if (g_next_gamestate != g_gamestate) {
		if (gamestate_changed(g_gamestate, g_next_gamestate)) {
			g_gamestate = g_next_gamestate;
		}
	}
}


static void draw(struct scene_battle_s *battle, struct engine *engine) {
	// draw terrain
	gbuffer_bind(g_gbuffer);
	gbuffer_clear(g_gbuffer);

	glEnable(GL_DEPTH_TEST);
	hexmap_draw(&g_hexmap, &g_camera);

	// draw neighbors & edges
	NVGcontext *vg = engine->vg;
	usize n_tiles = (usize)g_hexmap.w * g_hexmap.h;
	for (usize i = 0; i < n_tiles; ++i) {
		int edges_count = 0;
		while (g_hexmap.edges[i + n_tiles * edges_count] < n_tiles && edges_count < 6) {
			++edges_count;
		}
		float pct = edges_count / 6.0f;
		char text[32];
		sprintf(text, "%d", edges_count);

		vec2s wp = hexmap_index_to_world_position(&g_hexmap, i);
		vec3s p = (vec3s){ wp.x, 0.0f, wp.y };
		vec2s screen_pos = world_to_screen_camera(engine, &g_camera, GLM_MAT4_IDENTITY, p);

		nvgBeginPath(vg);
		nvgFillColor(vg, nvgRGBf(1.0f - pct, pct, 0.0f));
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(vg, 19.0f);
		nvgText(vg, screen_pos.x, screen_pos.y, text, NULL);

		nvgBeginPath(vg);
		nvgFontSize(vg, 12.0f);
		nvgFillColor(vg, nvgRGBf(1, 1, 1));
		char index_text[32];
		sprintf(index_text, "#%u", i);
		nvgText(vg, screen_pos.x + 10.0f, screen_pos.y + 10.0f, index_text, NULL);

		nvgStrokeWidth(vg, 3.0f);
		nvgStrokeColor(vg, nvgRGB(128, 0, 128));
		if (i == g_hexmap.highlight_tile_index) {
			for (int j = 0; j < edges_count; ++j) {
				usize neighbor = g_hexmap.edges[i + n_tiles * j];
				vec2s nwp = hexmap_index_to_world_position(&g_hexmap, neighbor);
				vec3s np = (vec3s){ nwp.x, 0.0f, nwp.y };
				vec2s n_screen_pos = world_to_screen_camera(engine, &g_camera, GLM_MAT4_IDENTITY, np);

				nvgBeginPath(vg);
				nvgMoveTo(vg, screen_pos.x, screen_pos.y);
				nvgLineTo(vg, n_screen_pos.x, n_screen_pos.y);
				nvgStroke(vg);
			}
		}
	}

	if (g_hexmap.highlight_tile_index < n_tiles) {
		nvgBeginPath(vg);

		usize index = hexmap_coord_to_index(&g_hexmap, g_move_goal);
		{
			vec2s wp = hexmap_index_to_world_position(&g_hexmap, index);
			vec3s p = (vec3s){{ wp.x, 0.0f, wp.y }};
			vec2s screen_pos = world_to_screen_camera(engine, &g_camera, GLM_MAT4_IDENTITY, p);
			nvgMoveTo(vg, screen_pos.x, screen_pos.y);
		}

		while (index < n_tiles) {
			vec2s wp = hexmap_index_to_world_position(&g_hexmap, index);
			vec3s p = (vec3s){{ wp.x, 0.0f, wp.y }};
			vec2s screen_pos = world_to_screen_camera(engine, &g_camera, GLM_MAT4_IDENTITY, p);
			nvgLineTo(vg, screen_pos.x, screen_pos.y);
			index = g_hexmap.tiles_flowmap[index];
		}

		nvgStrokeWidth(vg, 6.0);
		nvgStrokeColor(vg, nvgRGB(255, 70, 80));
		nvgStroke(vg);
	}

	ecs_run(g_world, ecs_id(system_draw_models), engine->dt, NULL);

	// Player model
	{
		// calculate matrices
		mat4 model = GLM_MAT4_IDENTITY_INIT;
		glm_translate(model, g_character_position.raw);
		//glm_rotate_y(model, glm_rad(sinf(g_engine->time_elapsed * 2.0f) * 80.0f), model);
		const float scale = 80.0f;
		glm_scale_uni(model, scale);

		// TODO: also do this for every node, as the model matrix changes...
		mat4 modelView = GLM_MAT4_IDENTITY_INIT;
		glm_mat4_mul(g_camera.view, model, modelView);
		mat3 normalMatrix = GLM_MAT3_IDENTITY_INIT;
		glm_mat4_pick3(modelView, normalMatrix);
		glm_mat3_inv(normalMatrix, normalMatrix);
		glm_mat3_transpose(normalMatrix);

		// Draw player
		shader_set_uniform_mat3(&g_player_model.shader, "u_normalMatrix", (float*)normalMatrix);
		model_draw(&g_player_model, &g_camera, model);

		// Draw enemy
		glm_mat4_identity(model);
		vec2s p = hexmap_index_to_world_position(&g_hexmap, 3 + 3 * g_hexmap.w);
		glm_translate(model, (vec3){ p.x, (fabsf(fminf(0.0f, sinf(g_engine->time_elapsed * 10.0f)))) * 50.0f, p.y });
		glm_rotate_y(model, glm_rad(30.0f), model);
		glm_scale_uni(model, scale);
		shader_set_uniform_mat3(&g_enemy_model.shader, "u_normalMatrix", (float*)normalMatrix);
		model_draw(&g_enemy_model, &g_camera, model);

		// Portrait model
		glm_mat4_identity(model);
		glm_translate(model, (vec3){ engine->window_width - 100.0f, engine->window_height - 240.0f, -300.0f });
		glm_rotate_x(model, glm_rad(10.0f + cosf(g_engine->time_elapsed) * 10.0f), model);
		glm_rotate_y(model, glm_rad(sinf(g_engine->time_elapsed) * 40.0f), model);
		glm_scale(model, (vec3){ 75.0f, 75.0f, 75.0f});
		const float pr = engine->window_pixel_ratio;
		glEnable(GL_SCISSOR_TEST);
		glScissor(engine->window_width * pr - 85.0f * pr, engine->window_height * pr - 85.0f * pr, 66 * pr, 66 * pr);
		model_draw(&g_player_model, &g_portrait_camera, model);
		glDisable(GL_SCISSOR_TEST);

	}
	glDisable(GL_DEPTH_TEST);

	gbuffer_unbind(g_gbuffer);

	// Combine scene with gbuffer
	engine_set_clear_color(0.34f, 0.72f, 0.98f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	background_draw(engine);
	gbuffer_display(g_gbuffer, engine);

	// drawing systems
	pipeline_reset(&g_cards_pipeline);
	g_cards_pipeline.texture = &g_cards_texture;

	ecs_run(g_world, ecs_id(system_draw_cards), engine->dt, NULL);

	// draw ui
	pipeline_reset(&g_ui_pipeline);
	draw_ui(&g_ui_pipeline);

	glEnable(GL_DEPTH_TEST);
	pipeline_draw_ortho(&g_ui_pipeline, g_engine->window_width, g_engine->window_height);
	glDisable(GL_DEPTH_TEST);
}

void on_callback(struct scene_battle_s *battle, struct engine *engine, struct engine_event event) {
	switch (event.type) {
	case ENGINE_EVENT_WINDOW_RESIZED:
		gbuffer_resize(&g_gbuffer, engine->window_highdpi_width, engine->window_highdpi_height);
		camera_resize_projection(&g_camera, engine->window_width, engine->window_height);
		camera_resize_projection(&g_portrait_camera, engine->window_width, engine->window_height);
		g_handcards_updated = 1;
		break;
	case ENGINE_EVENT_MAX:
		assert("unhandled event!");
		break;
	};
}

void scene_battle_init(struct scene_battle_s *scene_battle, struct engine *engine) {
	// init scene base
	scene_init((struct scene_s *)scene_battle, engine);

	// init function pointers
	scene_battle->base.load        = (scene_load_fn)load;
	scene_battle->base.destroy     = (scene_destroy_fn)destroy;
	scene_battle->base.update      = (scene_update_fn)update;
	scene_battle->base.draw        = (scene_draw_fn)draw;
	scene_battle->base.on_callback = (scene_on_callback_fn)on_callback;
}


//
// private implementations
//

static int gamestate_changed(enum gamestate_battle old_state, enum gamestate_battle new_state) {
	if (new_state == GS_TURN_PLAYER_IN_PROGRESS) {
		console_log(g_engine, "Your turn!");
	}

	return 1;
}

static void update_gamestate(enum gamestate_battle state, float dt) {
	switch (state) {
	case GS_BATTLE_BEGIN:
		g_next_gamestate = GS_ROUND_BEGIN;
		break;
	case GS_ROUND_BEGIN: {
		const int number_of_cards = count_handcards();
		for (int i = number_of_cards; i < 5; ++i) {
			spawn_random_card();
		}

		g_next_gamestate = GS_TURN_PLAYER_BEGIN;
		break;
	}
	case GS_TURN_PLAYER_BEGIN: {
		int done = add_cards_to_hand(dt);
		if (done) {
			g_next_gamestate = GS_TURN_PLAYER_IN_PROGRESS;
		}
		break;
	}
	case GS_TURN_PLAYER_IN_PROGRESS: {
		struct input_drag_s *drag = &g_engine->input_drag;
		int is_on_button_end_turn = drag_in_rect(drag, g_button_end_turn.x, g_button_end_turn.y, g_button_end_turn.w, g_button_end_turn.h);
		int is_dragging_card = (g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card));

		if (!is_on_button_end_turn && !is_dragging_card && (drag->state == INPUT_DRAG_BEGIN || drag->state == INPUT_DRAG_IN_PROGRESS)) {
			// Highlight
			vec3s p = screen_to_world(g_engine->window_width, g_engine->window_height, g_camera.projection, g_camera.view, g_engine->input_drag.x, g_engine->input_drag.y);
			usize index = hexmap_world_position_to_index(&g_hexmap, (vec2s){ .x=p.x, .y=p.z });
			hexmap_set_tile_effect(&g_hexmap, index, HEXMAP_TILE_EFFECT_HIGHLIGHT);

			// Pathfind
			g_move_goal = hexmap_world_position_to_coord(&g_hexmap, (vec2s){ .x=p.x, .y=p.z });
			hexmap_find_path(&g_hexmap, (struct hexcoord){3, 4}, g_move_goal);
		}
		if (drag->state == INPUT_DRAG_END && is_on_button_end_turn) {
			g_next_gamestate = GS_TURN_PLAYER_END;
		}
		break;
	}
	case GS_TURN_PLAYER_END:
		g_next_gamestate = GS_ROUND_END;
		break;
	case GS_TURN_BEGIN:
		break;
	case GS_TURN_IN_PROGRESS:
		break;
	case GS_TURN_END:
		break;
	case GS_ROUND_END:
		g_next_gamestate = GS_ROUND_BEGIN;
		break;
	case GS_BATTLE_END:
		break;
	};
}

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

static void draw_ui(pipeline_t *pipeline) {
	drawcmd_t cmd;

	// Frame
	cmd = DRAWCMD_INIT;
	cmd.size.x = 96;
	cmd.size.y = 96;
	cmd.position.x = g_engine->window_width - 96 - 4;
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
	int hp = 64.0f * fabsf(cosf(g_engine->time_elapsed));
	cmd = DRAWCMD_INIT;
	cmd.size.x = 48;
	cmd.size.y = hp * 3.0f;
	cmd.position.x = 1;
	cmd.position.y = 4;
	cmd.position.z = 0.1f;
	drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, 48, 0, 16, hp);
	pipeline_emit(&g_ui_pipeline, &cmd);

	// Draw "End turn" button
	if (g_gamestate == GS_TURN_PLAYER_IN_PROGRESS) {
		int is_on_button_end_turn = drag_in_rect(&g_engine->input_drag, g_button_end_turn.x, g_button_end_turn.y, g_button_end_turn.w, g_button_end_turn.h);

		NVGcontext *vg = g_engine->vg;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, g_button_end_turn.x, g_button_end_turn.y, g_button_end_turn.w, g_button_end_turn.h, 8.0f);
		if (is_on_button_end_turn) {
			nvgFillColor(vg, nvgRGB(85, 25, 25));
		} else {
			nvgFillColor(vg, nvgRGB(55, 10, 10));
		}
		nvgFill(vg);

		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(vg, 19.0f);
		nvgFillColor(vg, nvgRGBf(0.97f, 0.92f, 0.92f));
		nvgText(vg, g_button_end_turn.x + g_button_end_turn.w * 0.5f, g_button_end_turn.y + g_button_end_turn.h * 0.5f - 3.0f, "End turn", NULL);

		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(vg, 11.0f);
		nvgFillColor(vg, nvgRGB(92, 35, 35));
		nvgText(vg, g_button_end_turn.x + g_button_end_turn.w * 0.5f, g_button_end_turn.y + g_button_end_turn.h * 0.5f + 16.0f, "(X cards left)", NULL);
	}
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

	ecs_entity_t card_entity = info->entity;

	const c_card *card = ecs_get(g_world, card_entity, c_card);
	console_log(g_engine, "Played card: \"%s\"...", card->name);

	vec3s world_position = screen_to_world(
			g_engine->window_width, g_engine->window_height, g_camera.projection, g_camera.view,
			g_engine->input_drag.x, g_engine->input_drag.y);

	if (card->image_id == 0) {
		for (int i = 0; i < 20; ++i) {
			ecs_entity_t e = ecs_new_id(g_world);
			ecs_set(g_world, e, c_pos3d, { .x=world_position.x, .y=world_position.y, .z=world_position.z });
			ecs_set(g_world, e, c_model, {
				.model_index=2, .scale=200.0f + 100.0f * rng_f() });
			ecs_set(g_world, e, c_velocity, {
				.vel={{rng_f() * 20.0f - 10.0f, 10.0f + rng_f() * 10.0f, rng_f() * 20.0f - 10.0f}}});
		}
	} else if (card->image_id == 1) {
		for (int i = 0; i < 30; ++i) {
			ecs_entity_t e = ecs_new_id(g_world);
			ecs_set(g_world, e, c_pos3d, { .x=world_position.x, .y=world_position.y, .z=world_position.z });
			ecs_set(g_world, e, c_model, {
				.model_index=0, .scale=300.0f });
			ecs_set(g_world, e, c_velocity, {
				.vel={
					.x=cosf((i / 30.0f) * 3.1415926f) * 9.0f,
					.y=10.0f + rng_f() * 10.0f,
					.z=sinf((i / 15.0f) * 3.1415926f) * 20.0f,
				} });
		}
	} else if (card->image_id == 2) {
		for (int i = 0; i < 30; ++i) {
			ecs_entity_t e = ecs_new_id(g_world);
			ecs_set(g_world, e, c_pos3d, { .x=world_position.x, .y=world_position.y, .z=world_position.z });
			ecs_set(g_world, e, c_model, {
				.model_index=1, .scale=150.0f + 150.0f * rng_f() });
			ecs_set(g_world, e, c_velocity, {
				.vel={
					.x=cosf(((i*2.0f) / 30.0f) * M_PI) * 8.0f,
					.y=10.0f + rng_f() * 10.0f,
					.z=sinf(((i*2.0f) / 30.0f) * M_PI) * 20.0f,
				} });
		}
	}

	// remove card
	ecs_delete(g_world, g_selected_card);
	g_selected_card = 0;
}

static int count_handcards(void) {
	int count = 0;

	ecs_filter_t *filter = ecs_filter_init(g_world, &(ecs_filter_desc_t){
		.terms = {
			{ .id = ecs_id(c_card) },
		},
	});

	ecs_iter_t it = ecs_filter_iter(g_world, filter);
	while (ecs_filter_next(&it)) {
		count += it.count;
	}
	ecs_filter_fini(filter);

	return count;
}

static void interact_with_handcards(struct input_drag_s *drag) {
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
		c_pos2d *pos = ecs_get_mut(g_world, g_selected_card, c_pos2d);
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
}

int add_cards_to_hand(float dt) {
	int done = 0;

	const float card_add_speed = 0.25f;
	g_pickup_next_card += dt;
	if (g_pickup_next_card >= card_add_speed) {
		g_pickup_next_card -= card_add_speed;
		
		ecs_filter_t *filter = ecs_filter_init(g_world, &(ecs_filter_desc_t){
			.terms = {
				{ .id = ecs_id(c_card) },
				{ .id = ecs_id(c_pos2d), .oper = EcsNot },
				{ .id = ecs_id(c_handcard), .oper = EcsNot },
			},
		});

		ecs_iter_t it = ecs_filter_iter(g_world, filter);
		int has_entity = 0;
		while (ecs_filter_next(&it)) {
			if (it.count > 0) {
				has_entity = 1;
				ecs_entity_t e = it.entities[0];
				ecs_set(g_world, e, c_handcard, { .hand_space = 1.0f, .hand_target_pos = {0}, .is_selected = CS_NOT_SELECTED, .added_at_time=g_engine->time_elapsed });
				ecs_set(g_world, e, c_pos2d, { .x = g_engine->window_width, .y = g_engine->window_height * 0.9f });
				Mix_PlayChannel(-1, g_place_card_sfx, 0);
			}
		}
		if (!has_entity) {
			done = 1;
		}
		while (ecs_filter_next(&it)); // exhaust iterator. TODO: better way?
		ecs_filter_fini(filter);
	}

	return done;
}

static void spawn_random_card(void) {
	int n = rng_i() % 5;
	
	ecs_entity_t e = ecs_new_id(g_world);

	switch (n) {
	case 0:
		ecs_set(g_world, e, c_card, { .name="Fire Spell", .image_id=4, .icon_ids_count=1, .icon_ids={3} });
		break;
	case 1:
		ecs_set(g_world, e, c_card, { .name="Defend",     .image_id=2, .icon_ids_count=1, .icon_ids={2} });
		break;
	case 2:
		ecs_set(g_world, e, c_card, { .name="Meal",       .image_id=1, .icon_ids_count=1, .icon_ids={5} });
		break;
	case 3:
		ecs_set(g_world, e, c_card, { .name="Corruption", .image_id=5, .icon_ids_count=3, .icon_ids={3, 3, 4} });
		break;
	case 4:
		ecs_set(g_world, e, c_card, { .name="Attack", .image_id=0, .icon_ids_count=1, .icon_ids={1} });
		break;
	};
}

//
// systems implementation
//

static void system_move_cards(ecs_iter_t *it) {
	c_pos2d *positions = ecs_field(it, c_pos2d, 1);
	c_handcard *handcards = ecs_field(it, c_handcard, 2);

	for (int i = 0; i < it->count; ++i) {
		if (it->entities[i] == g_selected_card)
			continue;

		vec2s *p = &positions[i];
		vec2s *target = &handcards[i].hand_target_pos;
		glm_vec2_lerp(p->raw, target->raw, it->delta_time * 9.0f, p->raw);
	}
}

static void system_move_models(ecs_iter_t *it) {
	c_pos3d *pos_it = ecs_field(it, c_pos3d, 1);
	c_model *model_it = ecs_field(it, c_model, 2);
	c_velocity *velocity_it = ecs_field(it, c_velocity, 3);

	for (int i = 0; i < it->count; ++i) {
		c_pos3d *pos = &pos_it[i];
		c_model *model = &model_it[i];
		c_velocity *velocity = &velocity_it[i];

		glm_vec3_add(pos->raw, velocity->vel.raw, pos->raw);

		vec3 drag = {0.96f, 1.0f, 0.92f};
		glm_vec3_mul(velocity->vel.raw, drag, velocity->vel.raw);
		velocity->vel.y -= 0.8f;

		if (pos->y < 0.0f && velocity->vel.y < 0.0f) {
			pos->y = 0.0f;
			velocity->vel.y = fabsf(velocity->vel.y) * 0.6f;
			if (velocity->vel.y < 5.0f) {
				velocity->vel.y = 0.0f;
			}
		}
	}
}

static void system_draw_cards(ecs_iter_t *it) {
	c_card *cards = ecs_field(it, c_card, 1);
	c_handcard *handcards = NULL;
	c_pos2d *positions = ecs_field(it, c_pos2d, 3);

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

static void system_draw_models(ecs_iter_t *it) {
	c_pos3d *pos_it = ecs_field(it, c_pos3d, 1);
	c_model *models_it = ecs_field(it, c_model, 2);

	for (int i = 0; i < it->count; ++i) {
		c_pos3d *pos = &pos_it[i];
		c_model *model = &models_it[i];

		mat4 model_matrix = GLM_MAT4_IDENTITY_INIT;
		shader_set_uniform_mat3(&g_props_model[model->model_index].shader, "u_normalMatrix", (float*)model_matrix);
		glm_mat4_identity(model_matrix);
		glm_translate(model_matrix, pos->raw);
		glm_scale_uni(model_matrix, model->scale);
		model_draw(&g_props_model[model->model_index], &g_camera, model_matrix);
	}
}

static void observer_on_update_handcards(ecs_iter_t *it) {
	c_card *changed_cards = ecs_field(it, c_card, 1);
	c_handcard *changed_handcards = ecs_field(it, c_handcard, 2);

	g_handcards_updated = 1;
}

