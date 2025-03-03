#include "battle.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <SDL_net.h>
#include <SDL_mixer.h>
#include <nanovg.h>
#include <stb_ds.h>
#include <stb_image.h>
#include <stb_perlin.h>
#include <cglm/cglm.h>
#include <cJSON.h>
#include <flecs.h>
#include "engine.h"
#include "gl/opengles3.h"
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
#include "util/fs.h"
#include "util/util.h"
#include "util/str.h"

//
// structs & enums
//

enum card_selection {
	CS_NOT_SELECTED = 0,
	CS_SELECTED,
	CS_SELECTED_INITIAL,
};

enum gamestate_battle {
	GS_BATTLE_BEGIN           ,
	GS_ROUND_BEGIN            ,
	GS_TURN_ENTITY_BEGIN      , GS_TURN_PLAYER_BEGIN,
	GS_TURN_ENTITY_IN_PROGRESS, GS_TURN_PLAYER_IN_PROGRESS,
	GS_TURN_ENTITY_END        , GS_TURN_PLAYER_END,
	GS_ROUND_END              ,
	GS_BATTLE_END             ,
};

enum game_event_type {
	EVENT_PLAY_CARD,
	EVENT_MOVE_ENTITY,
	EVENT_ATTACK_ENTITY,
	EVENT_TYPE_MAX,
};

struct game_event {
	enum game_event_type type;
	union {
		struct {
			ecs_entity_t card;
			ecs_entity_t caused_by;
		} play_card;
		struct {
			ecs_entity_t entity;
			struct hexcoord goal;
		} move_entity;
		struct {
			ecs_entity_t attacker;
			ecs_entity_t victim;
			ecs_entity_t initiated_by_card;
		} attack_entity;
	};
};

enum effect_trigger {
	TRIGGER_DRAW_CARD,
	TRIGGER_PLAY_CARD,
	TRIGGER_DISCARD_CARD,
};

// components

// pos
typedef vec2s           c_pos2d;
typedef vec3s           c_pos3d;
typedef vec3s           c_tile_offset;
typedef struct hexcoord c_position;

typedef struct {
	model_t *model;
	float scale;
} c_model;

typedef struct {
	vec3s vel;
} c_velocity;

// general information about a card
enum card_kind {
	CARD_KIND_BASIC_ATTACK,
	CARD_KIND_ACTION,
};
typedef struct {
	char          *name;
	char          *description;
	int            image_id;
	int            icon_ids[8];
	int            icon_ids_count;
	enum card_kind kind;
	struct {
		int basic;
	} damage;
} c_card;

// a cards state when held in hand
const float HANDCARD_SPACE_DEFAULT = 1.0f;
const float HANDCARD_SPACE_DRAGGING = 0.4f;
typedef struct {
	vec2s hand_target_pos;
	float hand_space;
	enum card_selection is_selected;
	int   can_be_placed;
	float added_at_time;
} c_handcard;

typedef struct {
	u16 hp;
	u16 max_hp;
} c_health;

typedef struct {
	u8 _dummy;
} c_npc;

typedef struct {
	struct hexmap_path path;
	usize current_tile;
	float duration_per_tile;
	float percentage_to_next_tile;
} c_move_along_path;

typedef struct {
	int armor;
} c_statuseffect_armor;


ECS_COMPONENT_DECLARE(c_pos2d);
ECS_COMPONENT_DECLARE(c_pos3d);
ECS_COMPONENT_DECLARE(c_tile_offset);
ECS_COMPONENT_DECLARE(c_position);
ECS_COMPONENT_DECLARE(c_card);
ECS_COMPONENT_DECLARE(c_handcard);
ECS_COMPONENT_DECLARE(c_model);
ECS_COMPONENT_DECLARE(c_velocity);
ECS_COMPONENT_DECLARE(c_health);
ECS_COMPONENT_DECLARE(c_npc);
ECS_COMPONENT_DECLARE(c_move_along_path);
ECS_COMPONENT_DECLARE(c_statuseffect_armor);


//
// private functions
//

static void         recalculate_handcards(void);
static ecs_entity_t find_closest_handcard(float x, float y, float max_distance);
static int          order_handcards(ecs_entity_t e1, const void *data1, ecs_entity_t e2, const void *data2);
static void         draw_ui(pipeline_t *pipeline);
static void         draw_hud(pipeline_t *pipeline);
static void         on_game_event(struct game_event);
static void         on_game_event_play_card(struct game_event);
static int          count_cards(void);
static int          count_handcards_of_kind(enum card_kind);
static ecs_entity_t find_any_handcard_of_kind(enum card_kind);
static void         interact_with_handcards(struct input_drag_s *drag);
static int          add_cards_to_hand(float dt);
static ecs_entity_t spawn_random_card(void);
static int          gamestate_changed(enum gamestate_battle old_state, enum gamestate_battle new_state);
static void         update_gamestate(enum gamestate_battle state, float dt);
static void         highlight_reachable_tiles(struct hexcoord origin, usize distance);
static void         trigger_card_effect(c_card *, enum effect_trigger);

// systems
static void system_move_cards           (ecs_iter_t *);
static void system_move_models          (ecs_iter_t *);
static void system_draw_cards           (ecs_iter_t *);
static void system_draw_board_entities  (ecs_iter_t *);
static void system_draw_healthbars      (ecs_iter_t *);
static void system_enemy_turn           (ecs_iter_t *);
static void system_move_along_path      (ecs_iter_t *);
static void observer_on_update_handcards(ecs_iter_t *);

ECS_SYSTEM_DECLARE(system_move_cards);
ECS_SYSTEM_DECLARE(system_move_models);
ECS_SYSTEM_DECLARE(system_draw_cards);
ECS_SYSTEM_DECLARE(system_draw_board_entities);
ECS_SYSTEM_DECLARE(system_draw_healthbars);
ECS_SYSTEM_DECLARE(system_enemy_turn);
ECS_SYSTEM_DECLARE(system_move_along_path);


//
// vars
//
static struct engine *g_engine;
static struct gbuffer g_gbuffer;

// game state
static ecs_query_t          *g_ordered_handcards;
static int                   g_handcards_updated;
static texture_t             g_cards_texture;
static texture_t             g_ui_texture;
static shader_t              g_sprite_shader;
static shader_t              g_text_shader;
static shader_t              g_character_model_shader;
static pipeline_t            g_cards_pipeline;
static pipeline_t            g_ui_pipeline;
static pipeline_t            g_text_pipeline;
static ecs_world_t          *g_world;
static ecs_entity_t          g_selected_card;
static ecs_entity_t          g_player;
static fontatlas_t           g_card_font;
static model_t               g_player_model;
static model_t               g_enemy_model;
static model_t               g_props_model[4];
static float                 g_pickup_next_card;
static struct camera         g_camera;
static struct camera         g_portrait_camera;
static struct hexmap         g_hexmap;
static enum gamestate_battle g_gamestate;
static enum gamestate_battle g_next_gamestate;
static struct { float x, y, w, h; } g_button_end_turn;
static usize                 g_player_movement_this_turn;
static usize                 g_turn_count;
static usize                 g_base_cards_len;
static c_card               *g_base_cards;

// debug
static struct { float x, y, w, h; } g_debug_rect;
static int g_debug_draw_pathfinder;

// testing
static Mix_Chunk    *g_place_card_sfx;
static Mix_Chunk    *g_pick_card_sfx;
static Mix_Chunk    *g_slide_card_sfx;


//
// scene functions
//

static void load(struct scene_battle *battle, struct engine *engine) {
	g_engine = engine;
	rng_seed(time(NULL));

	g_handcards_updated = 0;
	g_selected_card = 0;
	g_pickup_next_card = 0.0f;
	g_gamestate = g_next_gamestate = GS_BATTLE_BEGIN;
	g_debug_draw_pathfinder = 0;
	g_turn_count = 0;

	g_button_end_turn.x = g_engine->window_width - 150.0f;
	g_button_end_turn.y = g_engine->window_height - 200.0f;
	g_button_end_turn.w = 130.0f;
	g_button_end_turn.h = 60.0f;
	g_debug_rect.x = 100.0f;
	g_debug_rect.y = 90.0f;
	g_debug_rect.w = 100.0f;
	g_debug_rect.h = 50.0f;

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

	hexmap_init(&g_hexmap, g_engine);

	// initialize camera
	camera_init_default(&g_camera, engine->window_width, engine->window_height);
	glm_translate(g_camera.view, (vec3){ -g_hexmap.tile_offsets.x * 3.25f, 0.0f, g_hexmap.tile_offsets.y * -3.0f });
	camera_init_default(&g_portrait_camera, engine->window_width, engine->window_height);
	glm_look((vec3){ 0.0f, 0.0f, 10.0f }, (vec3){ 0.0f, 0.0f, -1.0f }, (vec3){ 0.0f, 1.0f, 0.0f }, (float*)&g_portrait_camera.view);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// ecs
	g_world = ecs_init();
	ECS_COMPONENT_DEFINE(g_world, c_pos2d);
	ECS_COMPONENT_DEFINE(g_world, c_pos3d);
	ECS_COMPONENT_DEFINE(g_world, c_tile_offset);
	ECS_COMPONENT_DEFINE(g_world, c_position);
	ECS_COMPONENT_DEFINE(g_world, c_card);
	ECS_COMPONENT_DEFINE(g_world, c_handcard);
	ECS_COMPONENT_DEFINE(g_world, c_model);
	ECS_COMPONENT_DEFINE(g_world, c_velocity);
	ECS_COMPONENT_DEFINE(g_world, c_health);
	ECS_COMPONENT_DEFINE(g_world, c_npc);
	ECS_COMPONENT_DEFINE(g_world, c_move_along_path);
	ECS_COMPONENT_DEFINE(g_world, c_statuseffect_armor);
	ECS_SYSTEM_DEFINE(g_world, system_move_cards,          0, c_pos2d, c_handcard);
	ECS_SYSTEM_DEFINE(g_world, system_move_models,         0, c_pos3d, c_model, c_velocity);
	ECS_SYSTEM_DEFINE(g_world, system_draw_cards,          0, c_card, ?c_handcard, c_pos2d); _syntax_fix_label:
	ECS_SYSTEM_DEFINE(g_world, system_draw_board_entities, 0, c_position, c_model, ?c_tile_offset); _syntax_fix_label2:
	ECS_SYSTEM_DEFINE(g_world, system_draw_healthbars,     0, c_position, c_health, ?c_tile_offset); _syntax_fix_label3:
	ECS_SYSTEM_DEFINE(g_world, system_enemy_turn,          0, c_position, c_npc);
	ECS_SYSTEM_DEFINE(g_world, system_move_along_path,     0, c_position, c_tile_offset, c_move_along_path);

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

	// Add some entities
	{
		ecs_entity_t e;
		// campfire decoration
		struct hexcoord campfire_pos = { .x=3, .y=4 };
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_position, { .x=campfire_pos.x, .y=campfire_pos.y });
		ecs_set(g_world, e, c_model,    { .model=&g_props_model[3], .scale=10.0f });
		hexmap_tile_at(&g_hexmap, campfire_pos)->movement_cost = HEXMAP_MOVEMENT_COST_MAX;
		// enemy
		struct hexcoord enemy_pos = { .x=3, .y=3 };
		e = ecs_new_id(g_world);
		ecs_set(g_world, e, c_position,  { .x=enemy_pos.x, .y=enemy_pos.y });
		ecs_set(g_world, e, c_model,     { .model=&g_enemy_model, .scale=1.8f });
		ecs_set(g_world, e, c_health,    { .hp=8, .max_hp=8 });
		ecs_set(g_world, e, c_npc,       { ._dummy=1 });
		hexmap_tile_at(&g_hexmap, enemy_pos)->occupied_by = e;
		g_enemy_model.animation_index = 3;
		// player
		struct hexcoord player_pos = { .x=2, .y=5 };
		g_player = ecs_new_id(g_world);
		ecs_set(g_world, g_player, c_position,  { .x=player_pos.x, .y=player_pos.y });
		ecs_set(g_world, g_player, c_model,     { .model=&g_player_model, .scale=1.8f });
		ecs_set(g_world, g_player, c_health,    { .hp=7, .max_hp=10 });
		g_player_model.animation_index = 72;
		hexmap_tile_at(&g_hexmap, player_pos)->occupied_by = g_player;
	}

	// Character Shader
	shader_init_from_dir(&g_character_model_shader, "res/shader/model/gbuffer_pass/");

	// Load base cards
	{
		char *output;
		isize output_len;
		assert(FS_OK == fs_readfile("res/data/cards/base.json", &output, &output_len));
		cJSON *json = cJSON_ParseWithLength(output, output_len);
		free(output);

		assert(cJSON_IsArray(json));
		int number_of_cards = cJSON_GetArraySize(json);
		g_base_cards_len = number_of_cards;
		g_base_cards = malloc(number_of_cards * sizeof(*g_base_cards));
		for (usize i = 0; i < g_base_cards_len; ++i) {
			const cJSON *card             = cJSON_GetArrayItem(json, i);
			const cJSON *card_name        = cJSON_GetObjectItem(card, "name");
			const cJSON *card_description = cJSON_GetObjectItem(card, "description");
			const cJSON *card_face_id     = cJSON_GetObjectItem(card, "face_id");
			const cJSON *card_kind        = cJSON_GetObjectItem(card, "kind");
			const cJSON *card_damage      = cJSON_GetObjectItem(card, "damage");

			assert(cJSON_IsObject(card));
			assert(cJSON_IsString(card_name));
			assert(cJSON_IsString(card_description));
			assert(cJSON_IsNumber(card_face_id));
			assert(cJSON_IsString(card_kind));
			assert(strcmp(card_kind->valuestring, "basic_attack") == 0 || strcmp(card_kind->valuestring, "action") == 0);
			assert(cJSON_IsObject(card_damage) || cJSON_IsNull(card_damage));
			g_base_cards[i].name           = str_copy(card_name->valuestring);
			g_base_cards[i].description    = str_copy(card_description->valuestring);
			g_base_cards[i].image_id       = card_face_id->valueint;
			g_base_cards[i].icon_ids_count = 0;
			if (strcmp(card_kind->valuestring, "basic_attack") == 0) {
				g_base_cards[i].kind = CARD_KIND_BASIC_ATTACK;
			} else if (strcmp(card_kind->valuestring, "action") == 0) {
				g_base_cards[i].kind = CARD_KIND_ACTION;
			} else {
				assert(0 && "unknown card kind encountered!");
			}
			g_base_cards[i].damage.basic = 0;
			if (cJSON_IsObject(card_damage)) {
				const cJSON *damage_basic = cJSON_GetObjectItem(card_damage, "basic");
				assert(cJSON_IsNumber(damage_basic));
				g_base_cards[i].damage.basic = cJSON_GetNumberValue(damage_basic);
			}
		}

		cJSON_free(json);
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
		g_cards_pipeline.texture = &g_cards_texture;
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
		fontatlas_init(&g_card_font, engine);
		fontatlas_add_face(&g_card_font, "res/font/NotoSans-Regular.ttf",    9);
		fontatlas_add_face(&g_card_font, "res/font/NotoSans-Bold.ttf",       9);
		fontatlas_add_face(&g_card_font, "res/font/NotoSans-Italic.ttf",     9);
		fontatlas_add_face(&g_card_font, "res/font/NotoSans-BoldItalic.ttf", 9);
		// printable ascii characters
		fontatlas_add_ascii_glyphs(&g_card_font);

		shader_init_from_dir(&g_text_shader, "res/shader/text/");
		pipeline_init(&g_text_pipeline, &g_text_shader, 2048);
		g_text_pipeline.texture = &g_card_font.texture_atlas;
	}

	// background
	background_set_parallax("res/image/bg-clouds/%d.png", 4);
	background_set_parallax_offset(-0.7f);

	// audio
	g_place_card_sfx = Mix_LoadWAV("res/sounds/place_card.ogg");
	g_pick_card_sfx = Mix_LoadWAV("res/sounds/cardSlide5.ogg");
	g_slide_card_sfx = Mix_LoadWAV("res/sounds/cardSlide7.ogg");
}


static void destroy(struct scene_battle *battle, struct engine *engine) {
	Mix_FreeChunk(g_place_card_sfx);
	Mix_FreeChunk(g_pick_card_sfx);
	Mix_FreeChunk(g_slide_card_sfx);

	background_destroy();
	// TODO: Destroy remaining paths for all entities with a c_move_along_path component.
	hexmap_destroy(&g_hexmap);
	for (usize i = 0; i < g_base_cards_len; ++i) {
		const c_card card = g_base_cards[i];
		str_free(card.name);
		str_free(card.description);
	}
	free(g_base_cards);
	g_base_cards_len = 0;

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

static void update(struct scene_battle *battle, struct engine *engine, float dt) {
	// TODO: remove, test animation
	{
		static double t = 0.0;
		const double duration = 1.1;
		t += dt;
		if (t > duration) t -= duration;
		model_update_animation(&g_player_model, t);
		model_update_animation(&g_enemy_model, t);
	}

	if (g_handcards_updated) {
		g_handcards_updated = 0;
		recalculate_handcards();
	}

	update_gamestate(g_gamestate, dt);

	ecs_run(g_world, ecs_id(system_move_cards),      g_engine->dt, NULL);
	ecs_run(g_world, ecs_id(system_move_models),     g_engine->dt, NULL);
	ecs_run(g_world, ecs_id(system_move_along_path), g_engine->dt, NULL);

	if (g_next_gamestate != g_gamestate) {
		if (gamestate_changed(g_gamestate, g_next_gamestate)) {
			g_gamestate = g_next_gamestate;
		}
	}
}


static void draw(struct scene_battle *battle, struct engine *engine) {
	// Draw Scene (Map & Entites)
	gbuffer_bind(g_gbuffer);
	gbuffer_clear(g_gbuffer);
	background_draw(engine);

	glEnable(GL_DEPTH_TEST);

	const c_position *player_coord = ecs_get(g_world, g_player, c_position);
	vec2s player_pos = hexmap_coord_to_world_position(&g_hexmap, *player_coord);
	hexmap_draw(&g_hexmap, &g_camera, (vec3){player_pos.x, 0.0f, player_pos.y});

	ecs_run(g_world, ecs_id(system_draw_board_entities), engine->dt, NULL);

	glDisable(GL_DEPTH_TEST);

	gbuffer_unbind(g_gbuffer);

	// Combine scene with gbuffer
	engine_set_clear_color(0.34f, 0.72f, 0.98f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gbuffer_display(g_gbuffer, &g_camera, engine);

	// Draw ui
	draw_ui(&g_ui_pipeline);
}

void on_callback(struct scene_battle *battle, struct engine *engine, struct engine_event event) {
	switch (event.type) {
	case ENGINE_EVENT_WINDOW_RESIZED:
		gbuffer_resize(&g_gbuffer, engine->window_highdpi_width, engine->window_highdpi_height);
		camera_resize_projection(&g_camera, engine->window_width, engine->window_height);
		camera_resize_projection(&g_portrait_camera, engine->window_width, engine->window_height);
		g_handcards_updated = 1;
		break;
	case ENGINE_EVENT_KEY:
		if (event.data.key.type == SDL_KEYDOWN && event.data.key.repeat == 0 && event.data.key.keysym.sym == SDLK_r) {
			console_log_ex(engine, CONSOLE_MSG_SUCCESS, 0.5f, "3 shaders reloaded.");
			shader_reload_source(&g_hexmap.tile_shader);
			shader_reload_source(&g_character_model_shader);
			shader_reload_source(&g_gbuffer.shader);
		}
		break;
	case ENGINE_EVENT_CLOSE_SCENE: {
		struct scene_menu *menu_scene = malloc(sizeof(struct scene_menu));
		scene_menu_init(menu_scene, engine);
		engine_setscene(engine, (struct scene_s *)menu_scene);
		break;
	}
	case ENGINE_EVENT_MAX:
	default:
		assert(0 && "unhandled event!");
		break;
	};
}

void scene_battle_init(struct scene_battle *scene_battle, struct engine *engine) {
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
	switch (new_state) {
		case GS_BATTLE_BEGIN:
			break;
		case GS_ROUND_BEGIN:
			break;
		case GS_TURN_PLAYER_BEGIN:
			console_log(g_engine, "Your turn!");
			g_player_movement_this_turn = 2;
			break;
		case GS_TURN_PLAYER_IN_PROGRESS:
			break;
		case GS_TURN_PLAYER_END:
			break;
		case GS_TURN_ENTITY_BEGIN:
			ecs_run(g_world, ecs_id(system_enemy_turn), g_engine->dt, NULL);
			break;
		case GS_TURN_ENTITY_IN_PROGRESS:
			break;
		case GS_TURN_ENTITY_END:
			break;
		case GS_ROUND_END:
			++g_turn_count;
			break;
		case GS_BATTLE_END:
			break;
	}

	return 1;
}

static void update_gamestate(enum gamestate_battle state, float dt) {
	c_position *player_coord = ecs_get_mut(g_world, g_player, c_position);
	switch (state) {
	case GS_BATTLE_BEGIN:
		g_next_gamestate = GS_ROUND_BEGIN;
		break;
	case GS_ROUND_BEGIN: {
		const int number_of_cards = count_cards();
		const int max_cards_to_draw_at_turn_start = 5;
		for (int i = number_of_cards; i < max_cards_to_draw_at_turn_start; ++i) {
			spawn_random_card();
		}

		g_next_gamestate = GS_TURN_PLAYER_BEGIN;
		break;
	}
	case GS_TURN_PLAYER_BEGIN: {
		int done = add_cards_to_hand(dt);
		if (done) {
			highlight_reachable_tiles(*player_coord, g_player_movement_this_turn);
			g_next_gamestate = GS_TURN_PLAYER_IN_PROGRESS;
		}
		break;
	}
	case GS_TURN_PLAYER_IN_PROGRESS: {
		(void)add_cards_to_hand(dt); // TODO: we are doing this here and in TURN_PLAYER_BEGIN...
		struct input_drag_s *drag = &g_engine->input_drag;
		int is_on_button_end_turn = drag_in_rect(drag, g_button_end_turn.x, g_button_end_turn.y, g_button_end_turn.w, g_button_end_turn.h);
		int is_dragging_card = (g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card));

		if (!is_on_button_end_turn && !is_dragging_card && drag->state == INPUT_DRAG_END) {
			// Highlight & pathfind
			vec3s p_begin = screen_to_world(g_engine->window_width, g_engine->window_height, g_camera.projection, g_camera.view, g_engine->input_drag.begin_x, g_engine->input_drag.begin_y);
			vec3s p_end = screen_to_world(g_engine->window_width, g_engine->window_height, g_camera.projection, g_camera.view, g_engine->input_drag.end_x, g_engine->input_drag.end_y);
			struct hexcoord click_begin_coord = hexmap_world_position_to_coord(&g_hexmap, (vec2s){ .x=p_begin.x, .y=p_begin.z });
			struct hexcoord click_end_coord = hexmap_world_position_to_coord(&g_hexmap, (vec2s){ .x=p_end.x, .y=p_end.z });
			if (hexmap_is_valid_coord(&g_hexmap, click_begin_coord) && hexmap_is_valid_coord(&g_hexmap, click_end_coord) && hexcoord_equal(click_begin_coord, click_end_coord)) {
				ecs_entity_t occupied_by = hexmap_tile_at(&g_hexmap, click_begin_coord)->occupied_by;
				if (occupied_by != 0 && ecs_is_valid(g_world, occupied_by)) {
					struct hexmap_path path_to_neighbor;
					enum hexmap_path_result path_found =
						hexmap_path_find_ex(&g_hexmap, *ecs_get(g_world, g_player, c_position), click_begin_coord, PATH_FLAGS_FIND_NEIGHBOR, &path_to_neighbor);

					if (path_found == HEXMAP_PATH_OK) {
						if (path_to_neighbor.distance_in_tiles == 0) {
							const int basic_attacks_in_hand = count_handcards_of_kind(CARD_KIND_BASIC_ATTACK);
							if (basic_attacks_in_hand == 0) {
								console_log_ex(g_engine, CONSOLE_MSG_ERROR, 2.0f, "No Basic Attacks in Hand!");
							} else if (basic_attacks_in_hand >= 1) {
								// TODO: let the player choose a card if multiple basic attacks in hand!
								console_log_ex(g_engine, CONSOLE_MSG_SUCCESS, 2.0f, "Attacking");
								ecs_entity_t card = find_any_handcard_of_kind(CARD_KIND_BASIC_ATTACK);
								assert(card != 0 && ecs_is_valid(g_world, card));
								on_game_event((struct game_event){ .type=EVENT_ATTACK_ENTITY, .attack_entity={
									.attacker=g_player, .victim=occupied_by, .initiated_by_card=card } });
							}
						} else if (path_to_neighbor.distance_in_tiles <= g_player_movement_this_turn) {
							console_log_ex(g_engine, CONSOLE_MSG_INFO, 2.0f, "Moving into attack range");
							on_game_event((struct game_event){ .type=EVENT_MOVE_ENTITY, .move_entity={ .entity=g_player, .goal=path_to_neighbor.goal } });
							// TODO: Also attack, after moving completed.
						} else {
							console_log_ex(g_engine, CONSOLE_MSG_ERROR, 2.0f, "Not enough movement.");
						}
					}
					hexmap_path_destroy(&path_to_neighbor);
				} else {
					on_game_event((struct game_event){ .type=EVENT_MOVE_ENTITY, .move_entity={ .entity=g_player, .goal=click_end_coord } });
				}
			}
		}
		interact_with_handcards(&g_engine->input_drag);
		int clicked_on_button_end_turn = drag_clicked_in_rect(drag, g_button_end_turn.x, g_button_end_turn.y, g_button_end_turn.w, g_button_end_turn.h);
		if (clicked_on_button_end_turn) {
			g_next_gamestate = GS_TURN_PLAYER_END;
		}
		break;
	}
	case GS_TURN_PLAYER_END:
		if (hexmap_is_valid_coord(&g_hexmap, *player_coord)) {
			hexmap_set_tile_effect(&g_hexmap, *player_coord, HEXMAP_TILE_EFFECT_NONE);
		}
		hexmap_clear_tile_effect(&g_hexmap, HEXMAP_TILE_EFFECT_MOVEABLE_AREA);
		g_next_gamestate = GS_TURN_ENTITY_BEGIN;
		break;
	case GS_TURN_ENTITY_BEGIN:
		g_next_gamestate = GS_TURN_ENTITY_IN_PROGRESS;
		break;
	case GS_TURN_ENTITY_IN_PROGRESS:
		g_next_gamestate = GS_TURN_ENTITY_END;
		break;
	case GS_TURN_ENTITY_END:
		g_next_gamestate = GS_ROUND_END;
		break;
	case GS_ROUND_END:
		g_next_gamestate = GS_ROUND_BEGIN;
		break;
	case GS_BATTLE_END:
		break;
	};
}

static void highlight_reachable_tiles(struct hexcoord origin, usize distance) {
	// Regenerate flow field
	usize flowfield[g_hexmap.w * g_hexmap.h];
	hexmap_generate_flowfield(&g_hexmap, origin, count_of(flowfield), flowfield);
	// highlight new movement range
	hexmap_clear_tile_effect(&g_hexmap, HEXMAP_TILE_EFFECT_MOVEABLE_AREA);
	if (distance >= 1) {
		hexmap_set_tile_effect(&g_hexmap, origin, HEXMAP_TILE_EFFECT_MOVEABLE_AREA);
	}
	for (usize i = 0; i < (usize)g_hexmap.w * g_hexmap.h; ++i) {
		struct hexcoord coord = hexmap_index_to_coord(&g_hexmap, i);
		usize distance_to_reachable = hexmap_flowfield_distance(&g_hexmap, coord, count_of(flowfield), flowfield);
		if (distance_to_reachable >= 1 && distance_to_reachable <= distance) {
			hexmap_set_tile_effect(&g_hexmap, coord, HEXMAP_TILE_EFFECT_MOVEABLE_AREA);
		}
	}
}

static void trigger_card_effect(c_card *card, enum effect_trigger trigger) {
	switch (trigger) {
		case TRIGGER_DRAW_CARD:
			break;
		case TRIGGER_PLAY_CARD: {
			// TODO: Find a better way to identify cards, image_id is obviously not the answer...
			if (card->image_id == 1) {
				c_health *health = ecs_get_mut(g_world, g_player, c_health);
				int heal_until_max = health->max_hp - health->hp;
				int heal_for = (heal_until_max > 2 ? 2 : heal_until_max);
				if (heal_for <= 0) {
					console_log_ex(g_engine, CONSOLE_MSG_ERROR, 1.0f, "Already at full HP");
				}
				health->hp += heal_for;
				ecs_modified(g_world, g_player, c_health);
				console_log_ex(g_engine, CONSOLE_MSG_SUCCESS, 1.0f, "Healed for %d HP", heal_for);
			} else if (card->image_id == 5) {
				spawn_random_card();
				spawn_random_card();
			}
			break;
		}
		case TRIGGER_DISCARD_CARD:
			console_log(g_engine, "Discarded Card %s", card->name);
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
	pipeline_reset(pipeline);
	pipeline_reset(&g_cards_pipeline);

	ecs_run(g_world, ecs_id(system_draw_healthbars), g_engine->dt, NULL);

	// Draw UI - borders & elements
	draw_hud(pipeline);

	glDisable(GL_DEPTH_TEST);
	pipeline_draw_ortho(pipeline, g_engine->window_width, g_engine->window_height);
	ecs_run(g_world, ecs_id(system_draw_cards), g_engine->dt, NULL);

	{ // Draw UI - Portrait
		// TODO: this doesn't write to gbuffer, but rather renders with no lighting.
		glEnable(GL_DEPTH_TEST);
		mat4 model = GLM_MAT4_IDENTITY_INIT;
		glm_translate(model, (vec3){ -g_engine->window_width / 40.0f + 2.4f, g_engine->window_height / 40.0f - 5.5f, 0.0f });
		glm_rotate_x(model, glm_rad(10.0f + cosf(g_engine->time_elapsed) * 10.0f), model);
		glm_rotate_y(model, glm_rad(sinf(g_engine->time_elapsed) * 40.0f), model);
		glm_scale_uni(model, 1.75f);
		const float pr = g_engine->window_pixel_ratio;
		glEnable(GL_SCISSOR_TEST);
		glScissor(15.0f * pr, g_engine->window_height - 81.0f * pr, 66 * pr, 66 * pr);
		model_draw(&g_player_model, &g_character_model_shader, &g_portrait_camera, model);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);

		glm_mat4_identity(model);
		glm_translate(model, (vec3){12, 90, 0});
		pipeline_set_transform(&g_text_pipeline, model);
		pipeline_reset(&g_text_pipeline);
		fontatlas_writef_ex(&g_card_font, &g_text_pipeline, 0, 0, "$1Movement: $B%d", g_player_movement_this_turn);
		pipeline_draw_ortho(&g_text_pipeline, g_engine->window_width, g_engine->window_height);
	}

	{ // Debug Text
		mat4 model = GLM_MAT4_IDENTITY_INIT;
		glm_translate(model, (vec3){g_debug_rect.x, g_debug_rect.y, 0.0f});
		pipeline_set_transform(&g_text_pipeline, model);
		pipeline_reset(&g_text_pipeline);
		fontatlas_writef_ex(&g_card_font, &g_text_pipeline, 0, g_debug_rect.w, "$1Test -> $BText $0$1$IText $BText.$0\nTest -> Text Text Text.");
		pipeline_draw_ortho(&g_text_pipeline, g_engine->window_width, g_engine->window_height);

		float corner_radius = 6.0f;
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		vec2 corner = { g_debug_rect.x + g_debug_rect.w, g_debug_rect.y + g_debug_rect.h };
		vec2 mouse = { mx, my };
		float dist_to_corner = glm_vec2_distance(corner, mouse);
		int near_corner = (dist_to_corner <= corner_radius * 1.75f);
		if (near_corner) {
			corner_radius = 9.0f;
		}
		int dragging_corner = (dist_to_corner <= 20.0f && INPUT_DRAG_IS_DOWN(g_engine->input_drag));
		if (dragging_corner) {
			g_debug_rect.w = (g_engine->input_drag.x - g_debug_rect.x);
			g_debug_rect.h = (g_engine->input_drag.y - g_debug_rect.y);
		}

		NVGcontext *vg = g_engine->vg;
		nvgBeginPath(vg);
		nvgStrokeWidth(vg, dragging_corner ? 2.0f : 1.0f);
		nvgStrokeColor(vg, nvgRGB(255, 255, 255));
		nvgRect(vg, g_debug_rect.x - 2.0f, g_debug_rect.y - 2.0f, g_debug_rect.w + 4.0f, g_debug_rect.h + 4.0f);
		nvgStroke(vg);

		nvgBeginPath(vg);
		if (dragging_corner) {
			nvgFillColor(vg, nvgRGB(255, 255, 255));
		} else {
			nvgFillColor(vg, nvgRGB(180, 170, 170));
		}
		nvgCircle(vg, g_debug_rect.x + g_debug_rect.w, g_debug_rect.y + g_debug_rect.h, corner_radius);
		nvgFill(vg);

		nvgBeginPath(vg);
		nvgStrokeColor(vg, nvgRGB(255, 255, 255));
		nvgCircle(vg, g_debug_rect.x + g_debug_rect.w, g_debug_rect.y + g_debug_rect.h, corner_radius);
		nvgStroke(vg);
	}
}

static void draw_hud(pipeline_t *pipeline) {
	drawcmd_t cmd;
	// Frame (portrait & healthbar)
	cmd = DRAWCMD_INIT;
	cmd.size.x = 64*3;
	cmd.size.y = 32*3;
	cmd.position.x = 3;
	cmd.position.y = 3;
	cmd.position.z = -0.9f;
	drawcmd_set_texture_subrect(&cmd, pipeline->texture, 64, 0, 64, 32);
	pipeline_emit(pipeline, &cmd);
	// health
	const c_health *player_health = ecs_get(g_world, g_player, c_health);
	float player_health_pct = (float)player_health->hp / player_health->max_hp;
	cmd = DRAWCMD_INIT;
	cmd.size.x = 36*3 * player_health_pct;
	cmd.size.y = 2*3;
	cmd.position.x = 3 + 75;
	cmd.position.y = 3 + 12;
	cmd.position.z = -0.9f;
	drawcmd_set_texture_subrect(&cmd, pipeline->texture, 80, 32, 36 * player_health_pct, 2);
	pipeline_emit(pipeline, &cmd);

	// status effects
	cmd = DRAWCMD_INIT;
	cmd.size.x = 32;
	cmd.size.y = 32;
	cmd.position.x = 92;
	cmd.position.y = 32;
	drawcmd_set_texture_subrect(&cmd, pipeline->texture, 1, 118, 32, 32);
	pipeline_emit(pipeline, &cmd);

	// menu button
	cmd = DRAWCMD_INIT;
	cmd.size.x = 48;
	cmd.size.y = 48;
	cmd.position.x = g_engine->window_width  - 48 - 6;
	cmd.position.y = 6;
	drawcmd_set_texture_subrect(&cmd, pipeline->texture, 64, 48, 16, 16);
	pipeline_emit(pipeline, &cmd);

	// Draw "End turn" button
	if (g_gamestate == GS_TURN_PLAYER_IN_PROGRESS) {
		int is_on_button_end_turn = drag_in_rect(&g_engine->input_drag, g_button_end_turn.x, g_button_end_turn.y, g_button_end_turn.w, g_button_end_turn.h);
		char button_text[16];
		snprintf(button_text, 16, "End turn %ld", g_turn_count);

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
		nvgText(vg, g_button_end_turn.x + g_button_end_turn.w * 0.5f, g_button_end_turn.y + g_button_end_turn.h * 0.5f - 3.0f, button_text, NULL);

		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(vg, 11.0f);
		nvgFillColor(vg, nvgRGB(92, 35, 35));
		nvgText(vg, g_button_end_turn.x + g_button_end_turn.w * 0.5f, g_button_end_turn.y + g_button_end_turn.h * 0.5f + 16.0f, "(X cards left)", NULL);
	}

	if (g_debug_draw_pathfinder) {
		// draw neighbors & edges
		NVGcontext *vg = g_engine->vg;
		usize n_tiles = (usize)g_hexmap.w * g_hexmap.h;
		for (usize i = 0; i < n_tiles; ++i) {
			int edges_count = 0;
			while (g_hexmap.edges[i + n_tiles * edges_count] < n_tiles && edges_count < 6) {
				++edges_count;
			}

			vec2s wp = hexmap_index_to_world_position(&g_hexmap, i);
			vec3s p = (vec3s){{ wp.x, 0.0f, wp.y }};
			vec2s screen_pos = world_to_screen_camera(g_engine, &g_camera, GLM_MAT4_IDENTITY, p);

			// movement cost (center)
			float movecost_pct = g_hexmap.tiles[i].movement_cost < HEXMAP_MOVEMENT_COST_MAX ? 1.0f : 0.0f;
			nvgBeginPath(vg);
			nvgFillColor(vg, nvgRGBf(1.0f - movecost_pct, movecost_pct, 0.0f));
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFontSize(vg, 12.0f);
			char movecost_text[32];
			if (hexmap_is_tile_obstacle(&g_hexmap, hexmap_index_to_coord(&g_hexmap, i))) {
				nvgFillColor(vg, nvgRGB(255, 0, 0));
				sprintf(movecost_text, "#");
			} else {
				sprintf(movecost_text, "%d", g_hexmap.tiles[i].movement_cost);
			}
			nvgText(vg, screen_pos.x, screen_pos.y, movecost_text, NULL);

			// neighbors (lower left)
			float neighbors_pct = edges_count / 6.0f;
			nvgBeginPath(vg);
			nvgFillColor(vg, nvgRGBf(1.0f - neighbors_pct, neighbors_pct, 0.0f));
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFontSize(vg, 9.0f);
			char text[32];
			sprintf(text, "N=%d", edges_count);
			nvgText(vg, screen_pos.x - 12.0f, screen_pos.y + 10.0f, text, NULL);

			// index (lower right)
			nvgBeginPath(vg);
			nvgFontSize(vg, 9.0f);
			nvgFillColor(vg, nvgRGBf(1, 1, 1));
			char index_text[32];
			sprintf(index_text, "#%lu", i);
			nvgText(vg, screen_pos.x + 12.0f, screen_pos.y + 10.0f, index_text, NULL);

			nvgStrokeWidth(vg, 3.0f);
			nvgStrokeColor(vg, nvgRGB(128, 0, 128));
		}
	}

}


static void on_game_event(struct game_event event) {
	switch (event.type) {
	case EVENT_PLAY_CARD:
		on_game_event_play_card(event);
		break;
	case EVENT_MOVE_ENTITY: {
		const c_position start = *ecs_get(g_world, event.move_entity.entity, c_position);
		struct hexmap_path path;
		if (HEXMAP_PATH_OK == hexmap_path_find(&g_hexmap, start, event.move_entity.goal, &path)) {
			if (path.distance_in_tiles >= 1 && path.distance_in_tiles <= g_player_movement_this_turn) {
				hexmap_tile_at(&g_hexmap, event.move_entity.goal)->occupied_by = g_player;
				hexmap_tile_at(&g_hexmap, start)->occupied_by = 0;
				g_player_movement_this_turn -= path.distance_in_tiles;
				highlight_reachable_tiles(event.move_entity.goal, g_player_movement_this_turn);

				ecs_set(g_world, g_player, c_tile_offset, { .x=0.0f, .y=0.0f, .z=0.0f });
				ecs_set(g_world, g_player, c_move_along_path, { .path=path, .current_tile=0, .duration_per_tile=0.35f, .percentage_to_next_tile=0.0f });
				// We don't hexmap_path_destroy(), this is now managed by the animation system
			} else {
				hexmap_path_destroy(&path);
			}
		}
		break;
	}
	case EVENT_ATTACK_ENTITY: {
		assert(event.attack_entity.initiated_by_card != 0 && ecs_is_valid(g_world, event.attack_entity.initiated_by_card));
		const c_card *card = ecs_get(g_world, event.attack_entity.initiated_by_card, c_card);
		const int deals_damage = card->damage.basic;
		on_game_event((struct game_event){ .type=EVENT_PLAY_CARD, .play_card={ .caused_by=g_player, .card=event.attack_entity.initiated_by_card } });
		card = NULL; // gets removed by on_game_event()
		ecs_entity_t victim = event.attack_entity.victim;
		c_health *victim_health = ecs_get_mut(g_world, victim, c_health);
		// Determine attack damage
		const int victim_hp = (int)victim_health->hp;
		const int damage_dealt = GLM_MIN(victim_hp, deals_damage);
		//const int damage_overkill = deals_damage - damage_dealt;
		victim_health->hp -= damage_dealt;
		ecs_modified(g_world, victim, c_health);
		console_log_ex(g_engine, CONSOLE_MSG_ERROR, 1.0f, "Dealt %d damage.", damage_dealt);
		break;
	}
	case EVENT_TYPE_MAX: break;
	};
}


static void on_game_event_play_card(struct game_event event) {
	assert(event.type == EVENT_PLAY_CARD);
	assert(event.play_card.card != 0);
	assert(ecs_is_valid(g_world, event.play_card.card));

	ecs_entity_t card_entity = event.play_card.card;
	c_card *card = ecs_get_mut(g_world, card_entity, c_card);

	// TODO: Do something with card
	trigger_card_effect(card, TRIGGER_PLAY_CARD);

	// Old code, prevent placing card.
	/*
	c_handcard *handcard = ecs_get_mut(g_world, card_entity, c_handcard);
	if (card->image_id == 1) {
		c_health *health = ecs_get_mut(g_world, g_player, c_health);
		int heal_until_max = health->max_hp - health->hp;
		int heal_for = (heal_until_max > 2 ? 2 : heal_until_max);
		if (heal_for <= 0) {
			// TODO: Deny placing card, better way?
			handcard->can_be_placed = 0;
			handcard->is_selected = CS_NOT_SELECTED;
			handcard->hand_space = HANDCARD_SPACE_DEFAULT;
			ecs_modified(g_world, card_entity, c_handcard);
			g_selected_card = 0;

			console_log_ex(g_engine, CONSOLE_MSG_SUCCESS, 1.0f, "Already at full HP");
			return;
		}
		health->hp += heal_for;
		ecs_modified(g_world, g_player, c_health);
		console_log_ex(g_engine, CONSOLE_MSG_SUCCESS, 1.0f, "Healed for %d HP", heal_for);
	}
	*/

	// remove card
	ecs_delete(g_world, event.play_card.card);
	g_selected_card = 0;
}

static int count_cards(void) {
	ecs_filter_t *filter = ecs_filter_init(g_world, &(ecs_filter_desc_t){
		.terms = {
			{ .id = ecs_id(c_card) },
		},
	});
	ecs_iter_t it = ecs_filter_iter(g_world, filter);
	int count = 0;
	while (ecs_filter_next(&it)) {
		count += it.count;
	}
	ecs_filter_fini(filter);
	return count;
}

static int count_handcards_of_kind(enum card_kind kind) {
	int count = 0;
	ecs_iter_t it = ecs_query_iter(g_world, g_ordered_handcards);
	while (ecs_query_next(&it)) {
		c_card *cards = ecs_field(&it, c_card, 1);
		// c_handcard *handcards = ecs_field(&it, c_handcard, 2);
		for (int i = 0; i < it.count; ++i) {
			if (cards[i].kind == kind) {
				++count;
			}
		}
	}
	return count;
}

static ecs_entity_t find_any_handcard_of_kind(enum card_kind kind) {
	ecs_iter_t it = ecs_query_iter(g_world, g_ordered_handcards);
	while (ecs_query_next(&it)) {
		c_card *cards = ecs_field(&it, c_card, 1);
		// c_handcard *handcards = ecs_field(&it, c_handcard, 2);
		for (int i = 0; i < it.count; ++i) {
			if (cards[i].kind == kind) {
				while (ecs_query_next(&it)) { /* Exhaust iterator */ };
				return it.entities[i];
			}
		}
	}
	return 0;
}

static void interact_with_handcards(struct input_drag_s *drag) {
	// pick up card
	if (drag->state == INPUT_DRAG_BEGIN) {
		g_selected_card = find_closest_handcard(drag->begin_x, drag->begin_y, 110.0f);

		if (g_selected_card != 0 && ecs_is_valid(g_world, g_selected_card)) {
			c_handcard *hc = ecs_get_mut(g_world, g_selected_card, c_handcard);
			hc->hand_space = HANDCARD_SPACE_DRAGGING;
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
				on_game_event((struct game_event){ .type=EVENT_PLAY_CARD, .play_card={ .caused_by=g_player, .card=g_selected_card } });
			} else {
				hc->hand_space = HANDCARD_SPACE_DEFAULT;
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
			hc->hand_space = new_can_be_placed ? HANDCARD_SPACE_DRAGGING : HANDCARD_SPACE_DEFAULT;
			hc->can_be_placed = new_can_be_placed;
			ecs_modified(g_world, g_selected_card, c_handcard);
		}
	}
}

int add_cards_to_hand(float dt) {
	int done = 0;
	ecs_filter_t *filter = ecs_filter_init(g_world, &(ecs_filter_desc_t){
		.terms = {
			{ .id = ecs_id(c_card) },
			{ .id = ecs_id(c_pos2d), .oper = EcsNot },
			{ .id = ecs_id(c_handcard), .oper = EcsNot },
		},
	});

	const float card_add_speed = 0.25f;
	g_pickup_next_card += dt;
	if (g_pickup_next_card >= card_add_speed) {
		g_pickup_next_card -= card_add_speed;
		
		ecs_iter_t it = ecs_filter_iter(g_world, filter);
		int has_entity = 0;
		while (ecs_filter_next(&it)) {
			if (it.count > 0) {
				has_entity = 1;
				ecs_entity_t e = it.entities[0];
				ecs_set(g_world, e, c_handcard, { .hand_space = HANDCARD_SPACE_DEFAULT, .hand_target_pos = {0}, .is_selected = CS_NOT_SELECTED, .added_at_time=g_engine->time_elapsed });
				ecs_set(g_world, e, c_pos2d, { .x = g_engine->window_width, .y = g_engine->window_height * 0.9f });
				Mix_PlayChannel(-1, g_place_card_sfx, 0);
				trigger_card_effect(ecs_get_mut(g_world, e, c_card), TRIGGER_DRAW_CARD);
			}
		}
		if (!has_entity) {
			done = 1;
		}
		while (ecs_filter_next(&it)); // exhaust iterator. TODO: better way?
	}
	ecs_filter_fini(filter);

	return done;
}

static ecs_entity_t spawn_random_card(void) {
	int n = rng_i() % g_base_cards_len;
	ecs_entity_t e = ecs_new_id(g_world);
	c_card random_card = g_base_cards[n];
	ecs_set_ptr(g_world, e, c_card, &random_card);
	return e;
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
	c_pos3d    *pos_it      = ecs_field(it, c_pos3d,    1);
	c_model    *model_it    = ecs_field(it, c_model,    2);
	c_velocity *velocity_it = ecs_field(it, c_velocity, 3);

	for (int i = 0; i < it->count; ++i) {
		c_pos3d    *pos      = &pos_it[i];
		c_model    *model    = &model_it[i];
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
	c_card     *cards     = ecs_field(it, c_card,  1);
	c_handcard *handcards = NULL;
	c_pos2d    *positions = ecs_field(it, c_pos2d, 3);

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
			cmd_icon.size.x = cmd_icon.size.y = 16 * extra_scale;
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
		// Card Title
		glm_translate(model, (vec3){5, 64 * extra_scale, 0});
		pipeline_set_transform(&g_text_pipeline, model);
		pipeline_reset(&g_text_pipeline);
		fontatlas_writef_ex(&g_card_font, &g_text_pipeline, 0, 0, "$B%s$0", cards[i].name);
		pipeline_draw_ortho(&g_text_pipeline, g_engine->window_width, g_engine->window_height);
		// Card Description
		pipeline_reset(&g_text_pipeline);
		fontatlas_writef_ex(&g_card_font, &g_text_pipeline, 0, cmd_card.size.x - 10.0f, "\n%s", cards[i].description);
		pipeline_draw_ortho(&g_text_pipeline, g_engine->window_width, g_engine->window_height);
	}
}

static void system_draw_board_entities(ecs_iter_t *it) {
	c_position    *it_positions = ecs_field(it, c_position, 1);
	c_model       *it_models    = ecs_field(it, c_model,    2);
	c_tile_offset *it_offsets   = (ecs_field_is_set(it,     3) ? ecs_field(it, c_tile_offset, 3) : NULL);

	for (int i = 0; i < it->count; ++i) {
		c_position pos = it_positions[i];
		c_model *model = &it_models[i];
		c_tile_offset *tile_offset = (it_offsets == NULL ? NULL : &it_offsets[i]);

		vec2s offset = hexmap_coord_to_world_position(&g_hexmap, pos);
		vec3s world_pos = { .x=offset.x, .y=0.0f, .z=offset.y };
		if (tile_offset != NULL) {
			glm_vec3_add(world_pos.raw, tile_offset->raw, world_pos.raw);
		}

		mat4 model_matrix = GLM_MAT4_IDENTITY_INIT;
		shader_set_uniform_mat3(&g_character_model_shader, "u_normalMatrix", (float*)model_matrix);
		glm_translate(model_matrix, world_pos.raw);
		glm_scale_uni(model_matrix, model->scale);
		// TODO: either only draw characters here, or specify which shader to use?
		model_draw(model->model, &g_character_model_shader, &g_camera, model_matrix);

		if (g_debug_draw_pathfinder) {
			// Show components of this entity
			static char win_text[4096];
			ecs_entity_t e = it->entities[i];
			const ecs_type_t *type = ecs_get_type(g_world, e);
			if (type == NULL) continue;
			sprintf(win_text, "#%d components:\n", type->count);
			for (int j = 0; j < type->count; ++j) {
				ecs_id_t id = type->array[j];
				const char *name = ecs_get_name(g_world, id);
				sprintf(win_text + strlen(win_text), " - %s\n", name);
			}


			vec2s screen_pos = world_to_screen_camera(g_engine, &g_camera, GLM_MAT4_IDENTITY, world_pos);
			vec2s win_pos = { .x=screen_pos.x + 40.0f, .y=screen_pos.y - 70.0f };
			float win_max_width = 120.0f;
			NVGcontext *vg = g_engine->vg;
			// measure window size
			float win_bounds[4];
			nvgFontFaceId(vg, g_engine->font_monospace);
			nvgFontSize(vg, 10.0f);
			nvgTextBoxBounds(vg, 0.0f, 0.0f, win_max_width, win_text, NULL, win_bounds);
			// window
			nvgBeginPath(vg);
			nvgRect(vg, win_pos.x, win_pos.y, win_bounds[2] - win_bounds[0], win_bounds[3] - win_bounds[1]);
			nvgFillColor(vg, nvgRGBA(45, 40, 100, 0xA0));
			nvgFill(vg);
			// window border
			nvgBeginPath(vg);
			nvgRect(vg, win_pos.x, win_pos.y, win_bounds[2] - win_bounds[0], win_bounds[3] - win_bounds[1]);
			nvgStrokeWidth(vg, 3.5f);
			nvgStrokeColor(vg, nvgRGBA(35, 30, 95, 0xA0));
			nvgStroke(vg);
			// line to entity
			nvgBeginPath(vg);
			nvgStrokeWidth(vg, 3.5f);
			nvgMoveTo(vg, win_pos.x, win_pos.y + (win_bounds[3] - win_bounds[1]));
			nvgLineTo(vg, screen_pos.x, win_pos.y + 7.0f + (win_bounds[3] - win_bounds[1]));
			nvgStrokeColor(vg, nvgRGBA(35, 30, 95, 0xA0));
			nvgStroke(vg);
			// content
			nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
			nvgFillColor(vg, nvgRGB(255, 255, 255));
			nvgTextBox(vg, win_pos.x, win_pos.y, win_max_width, win_text, NULL);
			// restore previous font face
			nvgFontFaceId(vg, g_engine->font_default_bold);
		}
	}
}

static void system_draw_healthbars(ecs_iter_t *it) {
	c_position *it_position   = ecs_field(it, c_position, 1);
	c_health   *it_health     = ecs_field(it, c_health,   2);
	c_tile_offset *it_offsets = (ecs_field_is_set(it,     3) ? ecs_field(it, c_tile_offset, 3) : NULL);
	for (int i = 0; i < it->count; ++i) {
		c_position pos = it_position[i];
		c_health health = it_health[i];
		c_tile_offset *tile_offset = (it_offsets == NULL ? NULL : &it_offsets[i]);

		float health_pct = (float)health.hp / health.max_hp;

		// Enemy healthbar
		vec2s worldpos_xz = hexmap_coord_to_world_position(&g_hexmap, pos);
		vec3s worldpos = { .x=worldpos_xz.x, .y=0.0f, .z=worldpos_xz.y};
		if (tile_offset != NULL) {
			glm_vec3_add(worldpos.raw, tile_offset->raw, worldpos.raw);
		}
		vec2s screenpos = world_to_screen_camera(g_engine, &g_camera, GLM_MAT4_IDENTITY, worldpos);
		drawcmd_t cmd = DRAWCMD_INIT;
		cmd.size.x = 48;
		cmd.size.y = 12;
		cmd.position.x = floorf(screenpos.x - cmd.size.x * 0.5f);
		cmd.position.y = screenpos.y + 9.0f;
		cmd.position.z = 0.0f;
		// border
		drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, 16, 32, 48, 12);
		pipeline_emit(&g_ui_pipeline, &cmd);
		// health
		if (health_pct > 0.0f) {
			cmd.position.x += 3;
			cmd.position.y += 3;
			cmd.size.x = (42) * health_pct;
			cmd.size.y =   6;
			drawcmd_set_texture_subrect(&cmd, g_ui_pipeline.texture, 16, 48, 42 * health_pct, 6);
			pipeline_emit(&g_ui_pipeline, &cmd);
		}
	}
}

static void system_enemy_turn(ecs_iter_t *it) {
	c_position *it_position = ecs_field(it, c_position,  1);
	c_npc      *it_npc      = ecs_field(it, c_npc,       2);
	for (int i = 0; i < it->count; ++i) {
		ecs_entity_t e = it->entities[i];
		c_position *pos = &it_position[i];
		c_npc npc = it_npc[i];

		// Move to a random neighbor
		struct hexcoord random_neighbor;
		uint iterations = HEXMAP_MAX_NEIGHBORS;
		int start = rng_i() % (HEXMAP_N_LAST - HEXMAP_N_FIRST + 1);
		do {
			random_neighbor = hexmap_get_neighbor_coord(&g_hexmap, *pos, (start + iterations) % (HEXMAP_N_LAST + 1));
			if (iterations - 1 == 0) {
				console_log_ex(g_engine, CONSOLE_MSG_ERROR, 2.0f, "Enemy has 0 valid neighbors?!");
			}
		} while (iterations-- > 0 && (!hexmap_is_valid_coord(&g_hexmap, random_neighbor) || hexmap_is_tile_obstacle(&g_hexmap, random_neighbor)));

		if (hexmap_is_valid_coord(&g_hexmap, random_neighbor) && !hexmap_is_tile_obstacle(&g_hexmap, random_neighbor)) {
			struct hexmap_path path;
			enum hexmap_path_result result = hexmap_path_find(&g_hexmap, *pos, random_neighbor, &path);
			assert(result == HEXMAP_PATH_OK);
			ecs_set(g_world, e, c_tile_offset, { .x=0.0f, .y=0.0f, .z=0.0f });
			ecs_set(g_world, e, c_move_along_path, { .path=path, .current_tile=0, .duration_per_tile=0.5f, .percentage_to_next_tile=0.0f });

			hexmap_tile_at(&g_hexmap, random_neighbor)->occupied_by = e;
			hexmap_tile_at(&g_hexmap, *pos)->occupied_by = 0;
			hexmap_set_tile_effect(&g_hexmap, random_neighbor, HEXMAP_TILE_EFFECT_ATTACKABLE);
			hexmap_set_tile_effect(&g_hexmap, *pos, HEXMAP_TILE_EFFECT_NONE);
		} else {
			console_log_ex(g_engine, CONSOLE_MSG_ERROR, 2.0f, "Did not move?");
		}
	}
}

static void system_move_along_path(ecs_iter_t *it) {
	c_position        *it_position  = ecs_field(it, c_position,        1);
	c_tile_offset     *it_offset    = ecs_field(it, c_tile_offset,     2);
	c_move_along_path *it_move_path = ecs_field(it, c_move_along_path, 3);
	for (int i = 0; i < it->count; ++i) {
		ecs_entity_t       e         = it->entities[i];
		c_position        *pos       = &it_position[i];
		c_tile_offset     *offset    = &it_offset[i];
		c_move_along_path *move_path = &it_move_path[i];

		usize previous_tile = (move_path->current_tile == 0)
					? hexmap_coord_to_index(&g_hexmap, move_path->path.start)
					: hexmap_path_at(&move_path->path, move_path->current_tile - 1);
		usize next_tile = hexmap_path_at(&move_path->path, move_path->current_tile);

		if (move_path->percentage_to_next_tile >= move_path->duration_per_tile) {
			++move_path->current_tile;
			if (move_path->current_tile < move_path->path.distance_in_tiles) {
				// next node
				*pos = hexmap_index_to_coord(&g_hexmap, next_tile);
				move_path->percentage_to_next_tile -= move_path->duration_per_tile;
				ecs_set(g_world, e, c_tile_offset, { .x=0.0f, .y=0.0f, .z=0.0f });
			} else {
				// goal reached
				//*pos = move_path->path.goal;
				*pos = hexmap_index_to_coord(&g_hexmap, next_tile);
				hexmap_path_destroy(&move_path->path);
				ecs_remove(g_world, e, c_move_along_path);
				ecs_remove(g_world, e, c_tile_offset);
				continue;
			}
		}

		vec2s previous_tile_pos = hexmap_index_to_world_position(&g_hexmap, previous_tile);
		vec2s next_tile_pos = hexmap_index_to_world_position(&g_hexmap, next_tile);
		// calculate movement delta to next tile
		vec2 to_next;
		glm_vec2_sub(next_tile_pos.raw, previous_tile_pos.raw, to_next);
		glm_vec2_scale(to_next, move_path->percentage_to_next_tile / move_path->duration_per_tile, to_next);

		offset->x = to_next[0];
		offset->y = 0.0f;
		offset->z = to_next[1];

		move_path->percentage_to_next_tile += it->delta_time;
	}
}

static void observer_on_update_handcards(ecs_iter_t *it) {
	//c_card *changed_cards = ecs_field(it, c_card, 1);
	//c_handcard *changed_handcards = ecs_field(it, c_handcard, 2);

	g_handcards_updated = 1;
}

