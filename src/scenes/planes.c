#include "planes.h"

#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <nanovg.h>
#include <nuklear.h>
#include <flecs.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <stb_ds.h>
#include <stb_perlin.h>
#include "engine.h"
#include "gl/texture.h"
#include "gl/graphics2d.h"

//
// structs & enums
//

#define ZLAYER_ON_GROUND    0.1f
#define ZLAYER_ABOVE_GROUND 0.2f
#define ZLAYER_PARTICLES    0.3f
#define ZLAYER_ITEMS        0.4f
#define ZLAYER_PLANES       0.5f
#define ZLAYER_BULLETS      0.6f

enum faction {
	FACTION_NEUTRAL,
	FACTION_PLAYER,
	FACTION_ENEMY,
};

enum reward_type {
	REWARDOPTION_STATS_ADD,
	REWARDOPTION_STATS_MULT,
	REWARDOPTION_HEAL,
};

typedef struct {
	char *title;
	char *description;

	enum reward_type type;
	union {
		struct {
			float shoot_cd;
			float plane_speed;
			float bullet_damage;
			float bullet_speed;
		} modify_stats;
	} data;
} reward_t;

// components
typedef struct {
	int width, height;
	int *tiles;
} c_mapchunk;

typedef struct {
	vec2s p;
} c_pos;

typedef struct {
	vec2s vel;
	float speed;
	float damage;
} c_bullet;

typedef struct {
	float speed;
	float angle;

	float poof_cd_max;
	float poof_cd;
} c_plane;

typedef struct {
	ivec2 tile;
	ivec2 tile_size;
	float scale;
	float angle;

	float squish;
	float hurt;
	float z_layer;
} c_sprite;

typedef struct {
	float distance;
} c_shadow;

typedef struct {
	float shoot_cd_max;
	float shoot_cd;
	float plane_speed;
	float plane_turn_speed;
	float bullet_damage;
	float bullet_speed;
} c_player;

typedef struct {
	enum faction faction;
} c_faction;

typedef struct {
	float rotation;
	vec2 vel;
	float scale_factor;
} c_particle;

typedef struct {
	float seconds;
} c_despawn_after;

typedef struct {
	float hp;
	float max_hp;
} c_health;

//
// private functions
//

void system_move_plane(ecs_iter_t *);
void system_move_bullets(ecs_iter_t *);
void system_move_particles(ecs_iter_t *);
void system_despawn(ecs_iter_t *);
void system_unsquish(ecs_iter_t *);
void system_spawn_plane_effects(ecs_iter_t *);
void system_spawn_enemies(ecs_iter_t *);
void system_draw_sprites(ecs_iter_t *);
void system_draw_map(ecs_iter_t *);

void mapchunk_init(c_mapchunk *);
void mapchunk_destroy(c_mapchunk *);

void show_levelup_rewards(void);
void close_levelup_rewards(void);
void use_reward(reward_t *);
void collect_xp(float xp);

void hit_enemy(ecs_entity_t e_bullet, ecs_entity_t e_enemy, c_bullet *bullet, c_sprite *spr_enemy, c_health *hl_enemy, c_plane *pl_enemy);

//
// vars
//

static engine_t *g_engine; // engine reference for ecs callbacks

// assets
static int g_font;
static texture_t g_plane_tex;
static texture_t g_tiles_tex;
static primitive2d_t g_rect;
static primitive2d_t g_tile_rect;

// state
static int g_game_started;
static double g_game_timer;
static int g_picking_rewards;

static int g_level = 0;
static float g_xp = 0.0f;
static const int g_xp_per_level[14] = { 12, 24, 32, 69, 72, 84, 101, 196, 420, 666, 1024, 2250, 5000, 999999999 };
static reward_t g_levelup_rewards[3];

static float enemy_spawn_rate = 1.5f;
static float enemy_spawn_timer = 0.0f;

// entities
ecs_world_t *g_ecs;
ecs_query_t *g_enemies_query;
ecs_entity_t g_player;

// rewards
static const int g_reward_pool_length = 5;
static const reward_t g_reward_pool[] = {
	(reward_t) {
		.title = "Afterburner",
		.description = "Increase speed by 20%",
		.type = REWARDOPTION_STATS_MULT,
		.data.modify_stats = {
			.plane_speed   = 1.2f,
			.shoot_cd      = 1.0f,
			.bullet_damage = 1.0f,
			.bullet_speed  = 1.0f,
		},
	},
	(reward_t) {
		.title = "Fast shooter",
		.description = "Fire rate up!",
		.type = REWARDOPTION_STATS_ADD,
		.data.modify_stats = {
			.shoot_cd      = -0.333f,
			.plane_speed   = 0.0f,
			.bullet_damage = 0.0f,
			.bullet_speed  = 0.0f,
		},
	},
	(reward_t) {
		.title = "Minigun",
		.description = "Shoot fast, but less damage",
		.type = REWARDOPTION_STATS_MULT,
		.data.modify_stats = {
			.shoot_cd      = 0.35f,
			.plane_speed   = 1.0f,
			.bullet_damage = 0.4f,
			.bullet_speed  = 1.5f,
		},
	},
	(reward_t) {
		.title = "Silver Bullet",
		.description = "25% more damage",
		.type = REWARDOPTION_STATS_MULT,
		.data.modify_stats = {
			.shoot_cd      = 1.0f,
			.plane_speed   = 1.0f,
			.bullet_damage = 1.25f,
			.bullet_speed  = 1.0f,
		},
	},
	(reward_t) {
		.title = "Aerodynamics",
		.description = "Faster bullets",
		.type = REWARDOPTION_STATS_ADD,
		.data.modify_stats = {
			.bullet_speed = +0.65f,
		},
	},
};

ECS_COMPONENT_DECLARE(c_pos);
ECS_COMPONENT_DECLARE(c_bullet);
ECS_COMPONENT_DECLARE(c_plane);
ECS_COMPONENT_DECLARE(c_sprite);
ECS_COMPONENT_DECLARE(c_player);
ECS_COMPONENT_DECLARE(c_mapchunk);
ECS_COMPONENT_DECLARE(c_shadow);
ECS_COMPONENT_DECLARE(c_despawn_after);
ECS_COMPONENT_DECLARE(c_faction);
ECS_COMPONENT_DECLARE(c_particle);
ECS_COMPONENT_DECLARE(c_health);

ECS_SYSTEM_DECLARE(system_move_plane);
ECS_SYSTEM_DECLARE(system_move_bullets);
ECS_SYSTEM_DECLARE(system_move_particles);
ECS_SYSTEM_DECLARE(system_despawn);
ECS_SYSTEM_DECLARE(system_unsquish);
ECS_SYSTEM_DECLARE(system_spawn_plane_effects);
ECS_SYSTEM_DECLARE(system_spawn_enemies);
ECS_SYSTEM_DECLARE(system_draw_map);
ECS_SYSTEM_DECLARE(system_draw_sprites);

//
// scene functions
//

static void load(struct scene_planes_s *scene, struct engine_s *engine) {
	// configure engine
	engine_set_clear_color(0.87f, 0.96f, 0.96f);
	engine->console_visible = 0;
	glm_mat4_identity(engine->u_view);

	// setup state
	g_game_started    = 0;
	g_game_timer      = 0.0;
	g_picking_rewards = 0;
	g_level           = 0;
	g_xp              = 0.0f;

	// assets
	g_font = nvgCreateFont(engine->vg, "Miracode", "res/font/Miracode.ttf");
	struct texture_settings_s settings = TEXTURE_SETTINGS_INIT;
	settings.flip_y = 0;
	texture_init_from_image(&g_plane_tex, "res/sprites/planes.png", &settings);
	graphics2d_init_rect(&g_rect, 0.0f, 0.0f, 32.0f, 32.0f);
	graphics2d_set_texture_tile(&g_rect, &g_plane_tex, 32.0f, 32.0f, 0, 2);

	texture_init_from_image(&g_tiles_tex, "res/sprites/plane_tiles.png", &settings);
	graphics2d_init_rect(&g_tile_rect, 0.0f, 0.0f, 16.0f, 16.0f);
	graphics2d_set_texture_tile(&g_tile_rect, &g_tiles_tex, 16, 16, 1, 3);

	// ecs
	g_engine = engine;
	g_ecs = ecs_init();
	ECS_COMPONENT_DEFINE(g_ecs, c_pos);
	ECS_COMPONENT_DEFINE(g_ecs, c_bullet);
	ECS_COMPONENT_DEFINE(g_ecs, c_plane);
	ECS_COMPONENT_DEFINE(g_ecs, c_sprite);
	ECS_COMPONENT_DEFINE(g_ecs, c_player);
	ECS_COMPONENT_DEFINE(g_ecs, c_mapchunk);
	ECS_COMPONENT_DEFINE(g_ecs, c_shadow);
	ECS_COMPONENT_DEFINE(g_ecs, c_despawn_after);
	ECS_COMPONENT_DEFINE(g_ecs, c_faction);
	ECS_COMPONENT_DEFINE(g_ecs, c_particle);
	ECS_COMPONENT_DEFINE(g_ecs, c_health);

	ECS_SYSTEM_DEFINE(g_ecs, system_move_plane,          0, c_pos, c_plane);
	ECS_SYSTEM_DEFINE(g_ecs, system_move_bullets,        0, c_pos, c_bullet, c_faction);
	ECS_SYSTEM_DEFINE(g_ecs, system_move_particles,      0, c_pos, c_particle, c_sprite, ?c_shadow); dummy:
	ECS_SYSTEM_DEFINE(g_ecs, system_despawn,             0, c_despawn_after);
	ECS_SYSTEM_DEFINE(g_ecs, system_unsquish,            0, c_sprite);
	ECS_SYSTEM_DEFINE(g_ecs, system_spawn_plane_effects, 0, c_pos, c_plane);
	ECS_SYSTEM_DEFINE(g_ecs, system_spawn_enemies,       0, c_pos, c_player);
	ECS_SYSTEM_DEFINE(g_ecs, system_draw_map,            0, c_pos, c_mapchunk);
	ECS_SYSTEM_DEFINE(g_ecs, system_draw_sprites,        0, c_pos, c_sprite, ?c_shadow); dummy2: // fix weird query syyntax problems for my lsp

	g_enemies_query = ecs_query(g_ecs, {
		.filter.terms = {
			{ ecs_id(c_pos) }, { ecs_id(c_plane) }, { ecs_id(c_sprite) }, { ecs_id(c_faction) }, { ecs_id(c_health) }
		},
	});

	{ // spawn player
		ecs_entity_t e = ecs_new_id(g_ecs);
		g_player = e;
		ecs_set(g_ecs, e, c_pos, {
			.p.raw = { engine->window_width * 0.5f, engine->window_height * 0.5f }
		});
		ecs_set(g_ecs, e, c_plane, {
			.angle = 0.0f,
			.poof_cd_max = 0.4f,
			.poof_cd = 0.0f,
		});
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {0, 1},
			.tile_size = {32, 32},
			.scale = 2.0f,
			.angle = 0.0f,
			.z_layer = ZLAYER_PLANES + 0.01f,
		});
		ecs_set(g_ecs, e, c_shadow, { .distance = 32.0f });
		ecs_set(g_ecs, e, c_player, {
			.shoot_cd = 0.0f,
			.shoot_cd_max = 0.75f,
			.plane_speed = 1.0f,
			.plane_turn_speed = 1.4f,
			.bullet_damage = 1.0f,
			.bullet_speed = 6.0f,
		});
		ecs_set(g_ecs, e, c_faction, { .faction = FACTION_PLAYER });
	}

	{ // map
		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = {0.0f, 0.0f} });
		ecs_set(g_ecs, e, c_mapchunk, { 0, 0, NULL });
		
		c_mapchunk *chunk = ecs_get_mut(g_ecs, e, c_mapchunk);
		mapchunk_init(chunk);
		ecs_modified(g_ecs, e, c_mapchunk);
		
	}

	// planes
	for (int i = 0; i < 10; ++i) {
		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = {100.0f, 100.0f + (float)i * 50.0f} });
		ecs_set(g_ecs, e, c_plane, {
			.speed = 1.0f,
			.angle = 0.3f,
			.poof_cd = 0.0f,
			.poof_cd_max = 1.1f,
		});
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {rand() % 4, 5},
			.tile_size = {32, 32},
			.scale = 2.0f,
			.angle = 0.2f,
			.z_layer = ZLAYER_PLANES,
		});
		ecs_set(g_ecs, e, c_shadow, { .distance = 32.0f });
		ecs_set(g_ecs, e, c_faction, { .faction = FACTION_ENEMY });
		ecs_set(g_ecs, e, c_health, { .hp = 2.0f, .max_hp = 2.0f });
	}

	// items
	for (int i = 0; i < 3; ++i) {
		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = {370.0f, 100.0f + (float)i * 50.0f} });
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {12, 1},
			.tile_size = {16, 16},
			.scale = 2.0f,
			.angle = 0.0f,
			.z_layer = ZLAYER_ITEMS,
		});
	}

}

static void destroy(struct scene_planes_s *scene, struct engine_s *engine) {
	ecs_filter_t *f = ecs_filter(g_ecs, {
		.terms = {
			{ ecs_id(c_mapchunk) },
		}
	});

	ecs_iter_t it = ecs_filter_iter(g_ecs, f);
	while (ecs_filter_next(&it)) {
		c_mapchunk *chunks = ecs_field(&it, c_mapchunk, 1);

		for (int i = 0; i < it.count; i ++) {
			mapchunk_destroy(&chunks[i]);
		}
	}

	ecs_fini(g_ecs);
	texture_destroy(&g_plane_tex);
	texture_destroy(&g_tiles_tex);
}

static void update(struct scene_planes_s *scene, struct engine_s *engine, float dt) {
	if (!g_game_started) {
		if (engine->input_drag.state == INPUT_DRAG_BEGIN) {
			g_game_started = 1;
		}
		return;
	}

	static float gamespeed = 1.0f;
	if (g_picking_rewards) {
		gamespeed = glm_max(gamespeed - 0.025f, 0.0f);
		dt *= glm_ease_quart_in(gamespeed);
	} else {
		gamespeed = glm_min(gamespeed + 0.035f, 1.0f);
		dt *= glm_ease_quad_in(gamespeed);
	}

	g_game_timer += dt;

	// handle player controls
	if (ecs_is_alive(g_ecs, g_player)) {
		const c_pos *pp = ecs_get(g_ecs, g_player, c_pos);
		c_plane *pl = ecs_get_mut(g_ecs, g_player, c_plane);
		c_sprite *spr = ecs_get_mut(g_ecs, g_player, c_sprite);
		c_player *ply = ecs_get_mut(g_ecs, g_player, c_player);
		ply->shoot_cd -= dt;

		// turn player
		if (engine->input_drag.state == INPUT_DRAG_IN_PROGRESS) {
			const float turn_angle = 0.035f * dt * 60.0f;

			float angle_delta = 0.0f;
			pl->speed = ply->plane_turn_speed;
			if (engine->input_drag.x < engine->window_width * 0.5f) {
				angle_delta = -1.0f;
			} else {
				angle_delta = 1.0f;
			}

			spr->angle = pl->angle += turn_angle * angle_delta;

			ecs_modified(g_ecs, g_player, c_plane);
			ecs_modified(g_ecs, g_player, c_sprite);
		} else if (engine->input_drag.state == INPUT_DRAG_END) {
			pl->speed = ply->plane_speed;
			ecs_modified(g_ecs, g_player, c_plane);
		}

		// shoot bullet
		if (ply->shoot_cd <= 0.0f) {
			ply->shoot_cd = ply->shoot_cd_max;

			spr->squish = 1.0f;

			ecs_entity_t e = ecs_new_id(g_ecs);
			ecs_set(g_ecs, e, c_pos, { {{pp->p.x, pp->p.y} }});
			ecs_set(g_ecs, e, c_sprite, {
				.tile = {10, 0},
				.tile_size = {16, 16},
				.angle = pl->angle,
				.scale = 1.5f,
				.z_layer = ZLAYER_BULLETS,
			});
			ecs_set(g_ecs, e, c_despawn_after, { .seconds = 5.0f });
			ecs_set(g_ecs, e, c_bullet, {
				.vel.raw = {
					cosf(pl->angle), sinf(pl->angle)
				},
				.damage = ply->bullet_damage,
				.speed = ply->bullet_speed,
			});
			ecs_set(g_ecs, e, c_faction, { .faction = FACTION_PLAYER });
		}
	}

	ecs_run(g_ecs, ecs_id(system_move_plane),          dt, NULL);
	ecs_run(g_ecs, ecs_id(system_move_bullets),        dt, NULL);
	ecs_run(g_ecs, ecs_id(system_move_particles),      dt, NULL);
	ecs_run(g_ecs, ecs_id(system_despawn),             dt, NULL);
	ecs_run(g_ecs, ecs_id(system_unsquish),            dt, NULL);
	ecs_run(g_ecs, ecs_id(system_spawn_plane_effects), dt, NULL);

	enemy_spawn_timer += dt;
	while (enemy_spawn_timer >= enemy_spawn_rate) {
		enemy_spawn_timer -= enemy_spawn_rate;
		ecs_run(g_ecs, ecs_id(system_spawn_enemies), dt, NULL);
	}
}

static void draw(struct scene_planes_s *scene, struct engine_s *engine) {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	ecs_run(g_ecs, ecs_id(system_draw_map), 1.0f, NULL);
	ecs_run(g_ecs, ecs_id(system_draw_sprites), 1.0f, NULL);

	const float W = engine->window_width;
	const float H = engine->window_height;
	NVGcontext *vg = engine->vg;
	if (!g_game_started) {
		// draw controls

		const float pad = 14.0f;

		// left
		nvgBeginPath(vg);
		nvgRoundedRect(vg, pad, H * 0.4f + pad, W * 0.5f - pad * 1.5f, H * 0.6f - pad * 2.0f, 15.0f);
		nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.4f));
		nvgFill(vg);

		// right
		nvgBeginPath(vg);
		nvgRoundedRect(vg, W * 0.5f + pad * 0.5f, H * 0.4f + pad, W * 0.5f - pad * 1.5f, H * 0.6f - pad * 2.0f, 15.0f);
		nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.4f));
		nvgFill(vg);

		nvgTextAlign(vg, NVG_ALIGN_CENTER);
		nvgFontSize(vg, 28.0f);
		nvgFontFaceId(vg, g_font);
		// bg
		nvgFillColor(vg, nvgRGBf(0.0f, 0.0f, 0.0f));
		nvgFontBlur(vg, 1.0f);
		nvgText(vg, W * 0.25f - 2.0f, H * 0.65f + 2.0f, "Turn LEFT", NULL);
		nvgText(vg, W * 0.75f - 2.0f, H * 0.65f + 2.0f, "Turn RIGHT", NULL);
		// fg
		nvgFontBlur(vg, 0.0f);
		nvgFillColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
		nvgText(vg, W * 0.25f, H * 0.65f, "Turn LEFT", NULL);
		nvgText(vg, W * 0.75f, H * 0.65f, "Turn RIGHT", NULL);
	} else if (g_picking_rewards) {

		nvgBeginPath(vg);
		nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.2f));
		nvgRect(vg, 30.0f, 110.0f, W - 2*30.0f, H - 2*110.0f);
		nvgFill(vg);

		for (int i = 0; i < 3; ++i) {
			float bx = 30.0f + 15.0f;
			float by = 110.0f + 15.0f + (80.0f + 15.0f) * i;
			float bw = W - 2*30.0f - 2*15.0f;
			float bh = 80.0f;
			float mx = engine->input_drag.x;
			float my = engine->input_drag.y;
			int state = engine->input_drag.state;
			int is_hovering = (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh);
			
			if (state == INPUT_DRAG_END) {
				if (is_hovering) {
					// rewards!
					use_reward(&g_levelup_rewards[i]);
					close_levelup_rewards();
				}
			}

			reward_t rew = g_levelup_rewards[i];
			nvgBeginPath(vg);
			if (is_hovering) {
				nvgFillColor(vg, nvgRGBf(0.85f, 0.8f, 0.75f));
				nvgStrokeColor(vg, nvgRGBf(0.85f, 0.75f, 0.5f));
			} else {
				nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
				nvgStrokeColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
			}
			nvgStrokeWidth(vg, 4.0f);
			nvgRoundedRect(vg, bx, by, bw, bh, 4.0f);
			nvgFill(vg);
			nvgRoundedRect(vg, bx, by, bw, bh, 4.0f);
			nvgStroke(vg);

			nvgFontFaceId(vg, g_font);
			nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
			nvgFontSize(vg, 30.0f);
			nvgFillColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
			nvgText(vg, 30.0f + 15.0f + 20.0f, 110.0f + 15.0f + 50.0f + (80.0f + 15.0f) * i, rew.title, NULL);

			nvgFontSize(vg, 16.0f);
			nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
			nvgFillColor(vg, nvgRGBf(0.3f, 0.3f, 0.3f));
			nvgText(vg, 30.0f + 15.0f + 20.0f, 110.0f + 15.0f + 50.0f + (80.0f + 15.0f) * i, rew.description, NULL);
		}

	} else {
		static char game_timer[16];
		int ss = (int)(g_game_timer) % 60;
		int mm = (int)(g_game_timer / 60.0);
		int ms = (int)((g_game_timer - floorf(g_game_timer)) * 1000.0);
		snprintf(game_timer, 16, "%02d:%02d.%03d", mm, ss, ms);

		// draw menu bar
		nvgBeginPath(vg);
		nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.3f));
		nvgRoundedRect(vg, 12.0f, -40.0f, W - 2.0f * 12.0f, 40.0f + 42.0f, 4.0f);
		nvgFill(vg);

		// time
		// bg
		nvgFillColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgFontFaceId(vg, g_font);
		nvgFontSize(vg, 18.0f);
		nvgText(vg, 20.0f - 1.0f, 12.0f + 1.0f, game_timer, NULL);
		// fg
		nvgFillColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgFontSize(vg, 18.0f);
		nvgText(vg, 20.0f, 12.0f, game_timer, NULL);

		// draw level
		static char level[32];
		struct { float x, y, w, h } bounds;
		snprintf(level, 32, "lv.%d (%d/%d XP)", g_level, (int)g_xp, g_xp_per_level[g_level]);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		nvgFontSize(vg, 18.0f);
		nvgTextBounds(vg, 0.0f, 0.0f, level, NULL, (float*)&bounds);
		// bg
		nvgFillColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
		nvgFontSize(vg, 18.0f);
		nvgText(vg, W - 20.0f - bounds.w - 1.0f, 12.0f + 1.0f, level, NULL);
		// fg
		nvgFillColor(vg, nvgRGBf(1.0f, 1.0f, 1.0f));
		nvgText(vg, W - 20.0f - bounds.w, 12.0f, level, NULL);

	}
}

void scene_planes_init(struct scene_planes_s *scene, struct engine_s *engine) {
	// init scene base
	scene_init((struct scene_s *)scene, engine);
	
	// init function pointers
	scene->base.load    = (scene_load_fn)load;
	scene->base.destroy = (scene_destroy_fn)destroy;
	scene->base.update  = (scene_update_fn)update;
	scene->base.draw    = (scene_draw_fn)draw;
}


//
// private implementations
//

static void mapchunk_set(c_mapchunk *chunk, int x, int y, int tx, int ty) {
	chunk->tiles[x + y * chunk->width] = tx + ty * 16;
}
static int mapchunk_get(c_mapchunk *chunk, int x, int y, int *tx, int *ty) {
	if (x < 0 || y < 0 || x >= chunk->width || y >= chunk->height) {
		return 0;
	}

	int id = chunk->tiles[x + y * chunk->width];

	if (tx != NULL) {
		*tx = id % 16;
	}
	if (ty != NULL) {
		*ty = (int)(id / 16.0f);
	}

	return id;
}

void mapchunk_init(c_mapchunk *chunk) {
	chunk->width = 36;
	chunk->height = 54;
	chunk->tiles = calloc(chunk->width * chunk->height, sizeof(chunk->tiles[0]));

	int seed = time(0);
	srand(seed);

	for (int y = 0; y < chunk->height; ++y) {
		for (int x = 0; x < chunk->width; ++x) {
			// initialize all to water
			mapchunk_set(chunk, x, y, -1, -1);

			// islands
			float v = (
					  0.8f * stb_perlin_noise3_seed((float)x / 12.0f, (float)y / 12.0f, 0.0f, 0, 0, 0, seed)
					+ 0.2f * stb_perlin_noise3_seed((float)x /  6.0f, (float)y /  8.0f, 0.0f, 0, 0, 0, seed)
					);

			int is_land = (v <= 0.1f);

			if (is_land) {
				mapchunk_set(chunk, x, y, 2, 4);

				// vegetation
				float r = stb_perlin_noise3_seed((float)x / 8.0f, (float)y / 8.0f, 0.5f, 0, 0, 0, 256 * v);

				if (r > 0.4f) {
					mapchunk_set(chunk, x, y, 0, 4 + rand() % 2);
				}

				// houses
				float h = stb_perlin_noise3_seed((float)x / 10.0f, (float)y / 10.0f, 0.0f, 0, 0, 0, seed);
				int is_city = (h < -0.1f);

				h *= stb_perlin_noise3_seed((float)x / 2.5f, (float)y / 1.5f, 0.0f, 0, 0, 0, seed);
				int is_house = (h < -0.15f);

				h = stb_perlin_noise3_seed((float)x / 10.0f, (float)y / 10.0f, 0.4f, 0, 0, 0, seed)
					* stb_perlin_noise3_seed((float)x / 1.0f, (float)y / 3.0f + 0.5f, 0.4f, 0, 0, 0, seed);
				int is_field = (h < -0.1f);

				if (is_land && is_city) {
					if (is_house) {
						mapchunk_set(chunk, x, y, 0, 6 + rand() % 2);
					} else if (is_field) {
						mapchunk_set(chunk, x, y, 1, 9);
					}
				}
			}

		}
	}
	
	// second pass, terrain outline
	for (int y = 0; y < chunk->height; ++y) {
		for (int x = 0; x < chunk->width; ++x) {
			int tx, ty, id;
			id = mapchunk_get(chunk, x, y, &tx, &ty);
			if (id < 0) continue;

			int tiles[] = {
				// top
				mapchunk_get(chunk, x - 1, y - 1, NULL, NULL) < 0,
				mapchunk_get(chunk, x    , y - 1, NULL, NULL) < 0,
				mapchunk_get(chunk, x + 1, y - 1, NULL, NULL) < 0,
				// mid
				mapchunk_get(chunk, x - 1, y    , NULL, NULL) < 0,
				-1,
				mapchunk_get(chunk, x + 1, y    , NULL, NULL) < 0,
				// bottom
				mapchunk_get(chunk, x - 1, y + 1, NULL, NULL) < 0,
				mapchunk_get(chunk, x    , y + 1, NULL, NULL) < 0,
				mapchunk_get(chunk, x + 1, y + 1, NULL, NULL) < 0,
			};
			int tiles_count = tiles[0] + tiles[1] + tiles[2] + tiles[3] + tiles[5] + tiles[6] + tiles[7] + tiles[8];
			int top_count    = tiles[0] + tiles[1] + tiles[2];
			int left_count   = tiles[0] + tiles[3] + tiles[6];
			int right_count  = tiles[2] + tiles[5] + tiles[8];
			int bottom_count = tiles[6] + tiles[7] + tiles[8];

			int top = tiles[1];
			int left = tiles[3];
			int right = tiles[5];
			int bottom = tiles[7];

			int topleft = tiles[0];
			int topright = tiles[2];
			int botleft = tiles[6];
			int botright = tiles[8];

			int ox = -left + right;
			int oy = -top + bottom;

			if (ox == 0 && oy == 0) {
				if (topleft + topright + botleft + botright > 1) {
					continue;
				}

				if (topleft) {
					ox += 3;
				} else if (topright) {
					ox += 2;
				} else if (botleft) {
					ox += 3;
					oy -= 1;
				} else if (botright) {
					ox += 2;
					oy -= 1;
				}
			}

			if (ox || oy) {
				mapchunk_set(chunk, x, y, 2 + ox, 4 + oy);
			}
		}
	}
}

void mapchunk_destroy(c_mapchunk *chunk) {
	free(chunk->tiles);
	chunk->tiles = NULL;
}

void show_levelup_rewards(void) {
	g_picking_rewards = 1;

	for (int i = 0; i < 3; ++i) {
		g_levelup_rewards[i] = g_reward_pool[rand() % g_reward_pool_length];
	}
}

void close_levelup_rewards(void) {
	g_picking_rewards = 0;
}

void use_reward(reward_t *reward) {
	if (!ecs_is_alive(g_ecs, g_player)) {
		return;
	}

	c_plane *plane = ecs_get_mut(g_ecs, g_player, c_plane);
	c_player *player = ecs_get_mut(g_ecs, g_player, c_player);

	switch (reward->type) {
		case REWARDOPTION_STATS_ADD:
			plane->speed = (player->plane_speed += reward->data.modify_stats.plane_speed);
			player->shoot_cd_max += reward->data.modify_stats.shoot_cd;
			player->bullet_damage += reward->data.modify_stats.bullet_damage;
			player->bullet_speed += reward->data.modify_stats.bullet_speed;
			break;
		case REWARDOPTION_STATS_MULT:
			plane->speed = (player->plane_speed *= reward->data.modify_stats.plane_speed);
			player->shoot_cd_max *= reward->data.modify_stats.shoot_cd;
			player->bullet_damage *= reward->data.modify_stats.bullet_damage;
			player->bullet_speed *= reward->data.modify_stats.bullet_speed;
			break;
	}

	// TODO: only fire when really modified
	ecs_modified(g_ecs, g_player, c_plane);
	ecs_modified(g_ecs, g_player, c_player);
}

void collect_xp(float xp) {
	g_xp += xp;
	if (g_xp >= g_xp_per_level[g_level]) {
		g_xp -= g_xp_per_level[g_level];
		++g_level;

		show_levelup_rewards();
	}
}

void hit_enemy(ecs_entity_t e_bullet, ecs_entity_t e_enemy, c_bullet *bullet, c_sprite *spr_enemy, c_health *hl_enemy, c_plane *pl_enemy) {
	spr_enemy->hurt = 1.0f;
	hl_enemy->hp -= bullet->damage;

	// destroy bullet
	ecs_delete(g_ecs, e_bullet);

	// no hp left? also destroy plane
	if (hl_enemy->hp <= 0.0f) {
		// destroy plane, crash it into the ground
		ecs_set(g_ecs, e_enemy, c_particle, {
				.rotation=((rand() & 1) == 0 ? -0.1f : 0.1f),
				.vel={ cosf(pl_enemy->angle) * pl_enemy->speed, sinf(pl_enemy->angle) * pl_enemy->speed },
				.scale_factor = 0.999f,
				});
		ecs_remove(g_ecs, e_enemy, c_plane);
		ecs_set(g_ecs, e_enemy, c_despawn_after, { .seconds=1.0f });

		// TODO: better xp handling
		collect_xp(4.0f);
	}
}

// systems

void system_move_plane(ecs_iter_t *it) {
	c_pos *pos = ecs_field(it, c_pos, 1);
	c_plane *planes = ecs_field(it, c_plane, 2);

	for (int i = 0; i < it->count; ++i) {
		vec2 dir = {cosf(planes[i].angle), sinf(planes[i].angle)};
		glm_vec2_scale(dir, planes[i].speed * it->delta_time * 60.0f, dir);
		glm_vec2_add(pos[i].p.raw, dir, pos[i].p.raw);
	}
}

void system_move_bullets(ecs_iter_t *it_bullet) {
	c_pos *p_bullets = ecs_field(it_bullet, c_pos, 1);
	c_bullet *bullets = ecs_field(it_bullet, c_bullet, 2);
	c_faction *fac_bullets = ecs_field(it_bullet, c_faction, 3);

	for (int i = 0; i < it_bullet->count; ++i) {
		vec2 dir;
		glm_vec2_scale(bullets[i].vel.raw, bullets[i].speed * it_bullet->delta_time * 60.0f, dir);
		glm_vec2_add(p_bullets[i].p.raw, dir, p_bullets[i].p.raw);

		// collide with enemies
		ecs_iter_t it_enemy = ecs_query_iter(g_ecs, g_enemies_query);
		while (ecs_query_next(&it_enemy)) {
			c_pos     *p_planes      = ecs_field(&it_enemy, c_pos,     1);
			c_plane   *planes        = ecs_field(&it_enemy, c_plane,   2);
			c_sprite  *spr_planes    = ecs_field(&it_enemy, c_sprite,  3);
			c_faction *fac_planes    = ecs_field(&it_enemy, c_faction, 4);
			c_health  *health_planes = ecs_field(&it_enemy, c_health,  5);

			for (int j = 0; j < it_enemy.count; ++j) {
				if (fac_bullets[i].faction == fac_planes[j].faction) {
					continue;
				}

				vec2 to_plane;
				glm_vec2_sub(p_planes[j].p.raw, p_bullets[i].p.raw, to_plane);

				// hit!
				if (glm_vec2_norm2(to_plane) <= 25.0f * 25.0f) {
					hit_enemy(it_bullet->entities[i], it_enemy.entities[j], &bullets[i], &spr_planes[j], &health_planes[j], &planes[j]);
				}
			}
		}
	}

}

void system_move_particles(ecs_iter_t *it) {
	c_pos *pos = ecs_field(it, c_pos, 1);
	c_particle *particles = ecs_field(it, c_particle, 2);
	c_sprite *sprites = ecs_field(it, c_sprite, 3);
	c_shadow *shadows = NULL;

	for (int i = 0; i < it->count; ++i) {
		if (ecs_field_is_set(it, 4)) {
			shadows = ecs_field(it, c_shadow, 4);
		}

		// rotate
		sprites[i].angle += particles[i].rotation * 60.0f * it->delta_time;

		// move
		vec2 dir;
		glm_vec2_scale(particles[i].vel, 60.0f * it->delta_time, dir);
		glm_vec2_add(pos[i].p.raw, dir, pos[i].p.raw);

		// scale
		sprites[i].scale *= particles[i].scale_factor;

		if (shadows) {
			pos[i].p.y += 0.5f * 60.0f * it->delta_time;
			shadows[i].distance -= 0.5f * 60.0f * it->delta_time;
			shadows[i].distance = glm_max(0.0f, shadows[i].distance);
		}
	}
}
void system_despawn(ecs_iter_t *it) {
	c_despawn_after *ds = ecs_field(it, c_despawn_after, 1);

	for (int i = 0; i < it->count; ++i) {
		ds[i].seconds -= it->delta_time;
		if (ds[i].seconds <= 0.0f) {
			ecs_delete(g_ecs, it->entities[i]);
		}
	}
}

void system_unsquish(ecs_iter_t *it) {
	c_sprite *sprs = ecs_field(it, c_sprite, 1);

	for (int i = 0; i < it->count; ++i) {
		sprs[i].squish = glm_max(0.0f, sprs[i].squish - it->delta_time);
		sprs[i].hurt = glm_max(0.0f, sprs[i].hurt - it->delta_time);
	}
}

void system_draw_sprites(ecs_iter_t *it) {
	glDisable(GL_DEPTH_TEST);
	c_pos *pos = ecs_field(it, c_pos, 1);
	c_sprite *sprite = ecs_field(it, c_sprite, 2);
	c_shadow *shadow = NULL;

	for (int i = 0; i < it->count; ++i) {
		if (ecs_field_is_set(it, 3)) {
			shadow = ecs_field(it, c_shadow, 3);
		}

		ecs_field(it, c_shadow, 3);
		const float w = sprite[i].tile_size[0] * sprite[i].scale;
		const float h = sprite[i].tile_size[1] * sprite[i].scale;

		// squish
		float sq_amount = 0.3f;
		float sq_amount_inv = (1.0f - sq_amount);
		float sqw = w * sq_amount_inv + w * sq_amount * glm_ease_elast_out(1.0f - sprite[i].squish);
		float sqh = h * sq_amount_inv + h * sq_amount * glm_ease_elast_out(1.0f - sprite[i].squish);

		// hurt
		float hurt_color = glm_ease_exp_in(sprite[i].hurt);

		// draw shadow
		if (shadow != NULL) {
			float ox = -shadow[i].distance * 0.25f;
			float oy = shadow[i].distance;
			graphics2d_set_position_z(&g_rect, pos[i].p.x - sqw * 0.5f + ox, pos[i].p.y - sqh * 0.5f + oy, ZLAYER_ON_GROUND);
			graphics2d_set_scale(&g_rect, sqw, sqh);
			graphics2d_set_angle(&g_rect, sprite[i].angle + GLM_PI * 0.5f);
			graphics2d_set_texture_tile(&g_rect, &g_plane_tex, sprite[i].tile_size[0], sprite[i].tile_size[1], sprite[i].tile[0], sprite[i].tile[1]);
			graphics2d_set_color_mult(&g_rect, 0.0f, 0.0f, 0.0f, 0.25f);
			graphics2d_set_color_add(&g_rect, 0.0f, 0.0f, 0.0f, 0.0f);
			graphics2d_draw_primitive2d(g_engine, &g_rect);
		}
		
		// draw sprite
		graphics2d_set_position_z(&g_rect, pos[i].p.x - sqw * 0.5f, pos[i].p.y - sqh * 0.5f, sprite[i].z_layer);
		graphics2d_set_scale(&g_rect, sqw, sqh);
		graphics2d_set_angle(&g_rect, sprite[i].angle + GLM_PI * 0.5f);
		graphics2d_set_texture_tile(&g_rect, &g_plane_tex, sprite[i].tile_size[0], sprite[i].tile_size[1], sprite[i].tile[0], sprite[i].tile[1]);
		graphics2d_set_color_mult(&g_rect, 1.0f, 1.0f, 1.0f, 1.0f);
		graphics2d_set_color_add(&g_rect, hurt_color, hurt_color, hurt_color, 0.0f);
		graphics2d_draw_primitive2d(g_engine, &g_rect);
	}
}

void system_spawn_plane_effects(ecs_iter_t *it) {
	c_pos *ps = ecs_field(it, c_pos, 1);
	c_plane *planes = ecs_field(it, c_plane, 2);
	for (int i = 0; i < it->count; ++i) {
		c_pos *pos = &ps[i];
		c_plane *plane = &planes[i];

		if (plane->poof_cd >= plane->poof_cd_max) {
			plane->poof_cd -= plane->poof_cd_max;

			for (int j = 0; j < 4; ++j) {
				float angle = (rand() / (float)RAND_MAX) * GLM_PI * 2.0f;
				float speed = (rand() / (float)RAND_MAX) * 0.1f;

				vec2 rnd_dir = { cosf(angle) * speed, sinf(angle) * speed };
				vec2 inv_plane_dir = { -cosf(plane->angle) * 0.04f, -sinf(plane->angle) * 0.04f };

				vec2 dir;
				glm_vec2_add(rnd_dir, inv_plane_dir, dir);

				ecs_entity_t e = ecs_new_id(g_ecs);
				ecs_set(g_ecs, e, c_pos, { .p.x = pos->p.x, .p.y = pos->p.y });
				ecs_set(g_ecs, e, c_despawn_after, { .seconds = 2.0f });
				ecs_set(g_ecs, e, c_sprite, {
					.scale = 1.0f,
					.tile = {4, 0},
					.tile_size = {32, 32},
					.z_layer = ZLAYER_PARTICLES,
				});
				ecs_set(g_ecs, e, c_particle, {
					.rotation = 0.02f,
					.vel = { dir[0], dir[1] },
					.scale_factor = 0.99f,
				});
			}
		}

		plane->poof_cd += it->delta_time;
	}
}

void system_spawn_enemies(ecs_iter_t *it) {
	c_pos *ps = ecs_field(it, c_pos, 1);
	c_player *players = ecs_field(it, c_player, 2);

	for (int i = 0; i < it->count; ++i) {
		c_pos *pos = &ps[i];
		c_player *player = &players[i];

		// spawn enemies for each player?
		
		float y = (rand() / (double)RAND_MAX) * g_engine->window_height * 0.5f;

		ecs_entity_t e = ecs_new_id(g_ecs);
		ecs_set(g_ecs, e, c_pos, { .p.raw = {10.0f, y} });
		ecs_set(g_ecs, e, c_plane, {
			.angle = 0.3f,
			.speed = 1.0f,
			.poof_cd = 0.0f,
			.poof_cd_max = 1.1f,
		});
		ecs_set(g_ecs, e, c_sprite, {
			.tile = {rand() % 4, 5},
			.tile_size = {32, 32},
			.scale = 2.0f,
			.angle = 0.2f,
			.z_layer = ZLAYER_PLANES,
		});
		ecs_set(g_ecs, e, c_shadow, { .distance = 32.0f });
		ecs_set(g_ecs, e, c_faction, { .faction = FACTION_ENEMY });
		ecs_set(g_ecs, e, c_despawn_after, { .seconds=30.0f });
		ecs_set(g_ecs, e, c_health, { .hp = 2.0f, .max_hp = 2.0f });
	}
}

void system_draw_map(ecs_iter_t *it) {
	float alpha = cosf(g_engine->time_elapsed * 0.2f) * 0.08f;

	c_pos *ps = ecs_field(it, c_pos, 1);
	c_mapchunk *cs = ecs_field(it, c_mapchunk, 2);
	for (int i = 0; i < it->count; ++i) {

		for (int y = 0; y < cs[i].height; ++y) {
			for (int x = 0; x < cs[i].width; ++x) {
				int id = cs[i].tiles[x + y * cs[i].width];
				if (id < 0) {
					continue;
				}

				int tx = id % 16;
				int ty = (int)(id / 16);

				graphics2d_set_position(&g_tile_rect, ps[i].p.x + (x * 16), ps[i].p.y + (y * 16));
				graphics2d_set_texture_tile(&g_tile_rect, &g_tiles_tex, 16, 16, tx, ty);
				graphics2d_set_color_mult(&g_tile_rect, 1.0f, 1.0f, 1.0f, 0.75f + alpha);
				graphics2d_draw_primitive2d(g_engine, &g_tile_rect);
			}
		}
	}
}

